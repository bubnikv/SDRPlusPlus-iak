#include <gui/widgets/band_stack.h>
#include <gui/widgets/bandplan.h>
#include <gui/widgets/freq_memory.h>
#include <gui/gui.h>
#include <gui/tuner.h>
#include <config.h>
#include <core.h>
#include <radio_interface.h>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <utility>

namespace gui {
    BandStack bandStack;
}

// for getting / setting radio mode
//FIXME we may consider adding mode identifiers to demodulators such as USB for FT8.
static bool isRadioVFO(const std::string& vfoName) {
    return !vfoName.empty() && core::modComManager.interfaceExists(vfoName)
        && core::modComManager.getModuleName(vfoName) == "radio";
}

// One stable band_id's three rotating optional slots. Invalid persisted entries
// become empty without changing the positions of the other registers; there is
// no public-release migration path.
static BandRegisterSet readRegisters(
    const ConfigManager::ReadSection& mem,
    const bandplan::BandPlan_t* plan,
    const freq_input::BandMapping& mapping)
{
    BandRegisterSet regs;
    const json* entries = mem.section(mapping.bandId).peek();
    if (!entries || !entries->is_array()) { return regs; }
    const std::size_t count = std::min(entries->size(), BandRegisterSet::SIZE);
    for (std::size_t i = 0; i < count; i++) {
        const json& o = (*entries)[i];
        if (!o.is_object()) { continue; }
        const auto freqIt = o.find("freq");
        if (freqIt == o.end() || !freqIt->is_number()) { continue; }
        const double f = freqIt->get<double>();
        if (!std::isfinite(f) || f <= 0.0 || !plan ||
            !plan->findMappedSegmentAtFrequency(mapping, f))
        {
            continue;
        }

        int m = -1;
        const auto modeIt = o.find("mode");
        if (modeIt != o.end() && modeIt->is_string()) {
            m = radioModeFromName(modeIt->get_ref<const std::string&>().c_str());
        }
        regs.setSlot(i, { f, m });
    }
    return regs;
}

static void writeRegisters(
    ConfigManager::EditSection mem,
    const freq_input::BandMapping& mapping,
    const BandRegisterSet& registers)
{
    json out = json::array();
    for (std::size_t i = 0; i < registers.size(); i++) {
        const BandRegisterSet::Slot& slot = registers[i];
        if (slot) {
            json entry = { { "freq", slot->freq } };
            if (radioModeValid(slot->mode)) {
                entry["mode"] = radioModeName(slot->mode);
            }
            out.push_back(std::move(entry));
        }
        else { out.push_back(nullptr); }
    }
    // ConfigManager compares values, so writing an unchanged stack does not
    // dirty the config.
    mem.set(mapping.bandId, out);
}

// Resolve one stable band inside an explicit set of services. Prefer the
// supplied current service; if it has no match, retain the first match in
// source order.
// The picker may regroup services without changing this policy.
static const freq_input::BandMapping* resolveBandInServices(
    const bandplan::BandPlan_t* plan,
    double freq,
    freq_input::BandServiceSet services,
    freq_input::BandService currentService)
{
    if (!plan) { return nullptr; }

    const freq_input::BandMapping* fallback = nullptr;
    for (const auto& band : plan->bands) {
        const freq_input::BandMapping* mapping = band.resolved.mapping;
        if (!mapping ||
            !band.resolved.isBandOrSegment() ||
            !services.contains(band.resolved.service()) ||
            !band.containsFrequency(freq))
        {
            continue;
        }
        if (mapping->service == currentService) { return mapping; }
        if (!fallback) { fallback = mapping; }
    }
    return fallback;
}

// Returns whether the band was stored, not whether the document changed: the
// caller uses it to recognize a repeated tap on the band it's already on.
static bool updateTopRegister(
    ConfigManager::EditSection mem,
    const bandplan::BandPlan_t* plan,
    const freq_input::BandMapping& mapping,
    double freq,
    int mode)
{
    if (freq <= 0.0 || !plan ||
        !plan->findMappedSegmentAtFrequency(mapping, freq))
    {
        return false;
    }
    if (!radioModeValid(mode)) { mode = -1; }
    BandRegisterSet registers = readRegisters(mem, plan, mapping);
    registers.storeTop({ freq, mode });
    writeRegisters(mem, mapping, registers);
    return true;
}

// Write only to the stable ID the UI visibly selected. Membership checks its
// segment union; they must never search for a different ID opportunistically.
static bool storeVisibleBand(
    ConfigManager::EditSection mem,
    const bandplan::BandPlan_t* plan,
    const freq_input::BandMapping* activeMapping,
    double frequency,
    int mode)
{
    return activeMapping &&
        updateTopRegister(mem, plan, *activeMapping, frequency, mode);
}

BandRegisterPopupPreparation prepareBandRegisterPopup(
    const BandRegisterSet& storedRegisters,
    std::optional<BandRegister> current,
    std::optional<BandRegister> fallback,
    bool targetIsActive)
{
    BandRegisterSet stored = storedRegisters;
    bool storedChanged = false;
    if (targetIsActive && current) {
        stored.storeTop(*current);
        storedChanged = true;
    }
    else if (!stored.top() && fallback) {
        storedChanged = stored.seedTop(*fallback);
    }

    BandRegisterPopupSnapshot snapshot;
    snapshot.registers = std::move(stored);
    snapshot.canMaterializeEmpty = current.has_value();
    return { std::move(snapshot), storedChanged };
}

BandRegisterPopupSnapshot BandStack::openRegisters(
    const freq_input::BandMapping& mapping,
    double defaultFrequency,
    const freq_input::BandMapping* activeMapping)
{
    const bandplan::BandPlan_t* plan = gui::waterfall.bandplan;
    if (!plan) { return {}; }

    const double currentFrequency = (double)gui::freqSelect.frequency;
    const std::optional<BandRegister> current =
        captureRegister(plan, mapping, currentFrequency, currentMode());
    const std::optional<BandRegister> fallback =
        defaultRegister(plan, mapping, defaultFrequency);
    auto configAccess = core::configManager.edit();
    ConfigManager::EditSection mem =
        freq_memory::stackingRegisters(configAccess);
    BandRegisterPopupPreparation prepared = prepareBandRegisterPopup(
        readRegisters(mem, plan, mapping),
        current,
        fallback,
        activeMapping == &mapping);
    if (prepared.storedChanged) {
        writeRegisters(mem, mapping, prepared.snapshot.registers);
    }
    return std::move(prepared.snapshot);
}

const freq_input::BandMapping* BandStack::resolveBandForServices(
    freq_input::BandServiceSet services,
    freq_input::BandService preferredService,
    double frequency) const
{
    return resolveBandInServices(
        gui::waterfall.bandplan,
        frequency,
        services,
        preferredService);
}

// Service/frequency mode convention applied when the target segment carries no
// def_mode.
// Keep in sync with heuristic_mode() in scripts/enrich_bandplans.py.
int BandStack::heuristicMode(
    freq_input::BandService service,
    freq_input::BandFamily family,
    double frequency)
{
    if (service == freq_input::BandService::Amateur) {
        if (frequency <= 600000.0) { return RADIO_IFACE_MODE_CW; }                         // 2200 m / 630 m
        if (frequency >= 5200000.0 && frequency <= 5500000.0) { return RADIO_IFACE_MODE_USB; } // 60 m channels
        if (frequency < 10000000.0) { return RADIO_IFACE_MODE_LSB; }
        if (frequency < 100000000.0) { return RADIO_IFACE_MODE_USB; }                      // 30 m .. 6 m/4 m
        return RADIO_IFACE_MODE_NFM;                                                       // 2 m and up: repeaters
    }
    if (service == freq_input::BandService::Broadcast) {
        if (family == freq_input::BandFamily::WeatherBroadcast) {
            return RADIO_IFACE_MODE_NFM;
        }
        if (family == freq_input::BandFamily::TelevisionBroadcast) {
            return -1;
        }
        return (frequency >= 30000000.0) ? RADIO_IFACE_MODE_WFM : RADIO_IFACE_MODE_AM;
    }
    if (service == freq_input::BandService::Aviation) {
        if (family == freq_input::BandFamily::AviationSurveillance) {
            return -1;
        }
        return (frequency < 30000000.0) ? RADIO_IFACE_MODE_USB : RADIO_IFACE_MODE_AM;
    }
    if (service == freq_input::BandService::Maritime) {
        return (frequency < 30000000.0) ? RADIO_IFACE_MODE_USB : RADIO_IFACE_MODE_NFM;
    }
    return -1; // no mode change for other band types
}

int BandStack::resolvedMode(
    const bandplan::Band_t& segment,
    double frequency,
    int mode)
{
    if (radioModeValid(mode)) { return mode; }
    mode = radioModeFromName(segment.defMode.c_str());
    if (mode < 0) {
        mode = heuristicMode(
            segment.resolved.service(),
            segment.resolved.family(),
            frequency);
    }
    return mode;
}

std::optional<BandRegister> BandStack::captureRegister(
    const bandplan::BandPlan_t* plan,
    const freq_input::BandMapping& mapping,
    double frequency,
    int mode)
{
    if (frequency <= 0.0 || !plan) { return std::nullopt; }
    const bandplan::Band_t* segment =
        plan->findMappedSegmentAtFrequency(mapping, frequency);
    if (!segment) { return std::nullopt; }
    if (!radioModeValid(mode)) { mode = -1; }
    return BandRegister{ frequency, mode };
}

std::optional<BandRegister> BandStack::defaultRegister(
    const bandplan::BandPlan_t* plan,
    const freq_input::BandMapping& mapping,
    double frequency)
{
    if (frequency <= 0.0 || !plan) { return std::nullopt; }
    const bandplan::Band_t* segment =
        plan->findMappedSegmentAtFrequency(mapping, frequency);
    if (!segment) { return std::nullopt; }
    return BandRegister{ frequency, resolvedMode(*segment, frequency, -1) };
}

void BandStack::selectLegacySegment(
    const bandplan::Band_t& segment,
    const freq_input::BandMapping* activeMapping)
{
    if (segment.resolved.mapping ||
        !segment.resolved.isBandOrSegment() ||
        !segment.hasValidFrequencySpan())
    {
        return;
    }
    const int curMode = currentMode();
    const double currentFrequency = (double)gui::freqSelect.frequency;
    double targetFrequency = segment.defFreq;
    if (targetFrequency <= 0.0 ||
        !segment.containsFrequency(targetFrequency))
    {
        // Tune to center of a band segment, rounded to 1 kHz
        targetFrequency = (segment.start + segment.end) / 2.0;
        const double rounded = std::round(targetFrequency / 1000.0) * 1000.0;
        if (rounded > 0.0 && segment.containsFrequency(rounded))
            targetFrequency = rounded;
    }

    {
        auto configAccess = core::configManager.edit();
        storeVisibleBand(
            freq_memory::stackingRegisters(configAccess),
            gui::waterfall.bandplan,
            activeMapping,
            currentFrequency,
            curMode);
        ConfigManager::EditSection band = freq_memory::band(configAccess);
        band.set(freq_memory::PREFERRED_SERVICE, freq_input::bandServiceKey(segment.resolved.service()));
        freq_memory::root(configAccess).set(freq_memory::SELECTOR, freq_memory::SELECTOR_BAND);
    }

    applySegmentTarget(segment, targetFrequency, -1);
}

void BandStack::selectBand(
    const freq_input::BandMapping& mapping,
    double defaultFrequency,
    const freq_input::BandMapping* activeMapping)
{
    const bandplan::BandPlan_t* plan = gui::waterfall.bandplan;
    if (!plan) { return; }
    const int curMode = currentMode();
    const double currentFrequency = (double)gui::freqSelect.frequency;
    std::optional<BandRegister> target;

    {
        auto configAccess = core::configManager.edit();
        ConfigManager::EditSection root = freq_memory::root(configAccess);
        ConfigManager::EditSection band = root.section(freq_memory::SELECTOR_BAND);
        ConfigManager::EditSection mem = band.section(freq_memory::STACKING_REGISTERS);
        const bool couldRepeat = activeMapping == &mapping;
        BandRegisterSet registers;
        if (couldRepeat) {
            // Keep the potential repeat target's pre-write state because
            // storeVisibleBand() overwrites this same band's top.
            registers = readRegisters(mem, plan, mapping);
        }
        const bool storedSource = storeVisibleBand(
            mem,
            plan,
            activeMapping,
            currentFrequency,
            curMode);
        const bool repeatTap = storedSource && couldRepeat;

        band.set(freq_memory::PREFERRED_SERVICE, freq_input::bandServiceKey(mapping.service));
        root.set(freq_memory::SELECTOR, freq_memory::SELECTOR_BAND);

        if (repeatTap) {
            // Keep partial-stack push and full-stack roll in one tested
            // transition using the pre-write stack captured above.
            registers.repeatWithCurrent({
                currentFrequency,
                radioModeValid(curMode) ? curMode : -1
            });
            writeRegisters(mem, mapping, registers);
        }
        else {
            // Preserve the existing switch-band ordering: save the visible
            // source first, then load the target stack.
            registers = readRegisters(mem, plan, mapping);
        }

        target = registers.top();
        if (!target && !repeatTap) {
            target = defaultRegister(plan, mapping, defaultFrequency);
            if (target) {
                registers.seedTop(*target);
                writeRegisters(mem, mapping, registers);
            }
        }
    }

    if (target) { applyTarget(mapping, *target); }
}

BandRecallResult BandStack::recallRegister(
    const freq_input::BandMapping& mapping,
    int index,
    const freq_input::BandMapping* activeMapping,
    const BandRegisterPopupSnapshot& openedSnapshot)
{
    const bandplan::BandPlan_t* plan = gui::waterfall.bandplan;
    if (!plan || index < 0 || index >= (int)BandRegisterSet::SIZE) {
        return BandRecallResult::NoTarget;
    }
    // Could be -1 if current VFO is a decoder or there is no VFO active.
    const int curMode = currentMode();
    const double currentFrequency = (double)gui::freqSelect.frequency;
    std::optional<BandRegister> frozenTop;
    if (index == 0 && openedSnapshot.registers[0]) {
        const BandRegister& shown = *openedSnapshot.registers[0];
        if (plan->findMappedSegmentAtFrequency(mapping, shown.freq)) {
            frozenTop = shown;
        }
    }
    std::optional<BandRegister> materialization;
    BandRegister target;

    {
        auto configAccess = core::configManager.edit();
        ConfigManager::EditSection root = freq_memory::root(configAccess);
        ConfigManager::EditSection band = root.section(freq_memory::SELECTOR_BAND);
        ConfigManager::EditSection mem = band.section(freq_memory::STACKING_REGISTERS);

        BandRegisterSet registers = readRegisters(mem, plan, mapping);
        if (!frozenTop && !registers[(std::size_t)index]) {
            // EditSection paths are lazy. Returning here performs no config
            // mutation because no writer has been called yet.
            materialization = captureRegister(
                plan,
                mapping,
                currentFrequency,
                curMode);
            if (!materialization) { return BandRecallResult::NoTarget; }
        }

        const bool materializingActiveTarget =
            materialization && activeMapping == &mapping;
        if (!materializingActiveTarget) {
            storeVisibleBand(
                mem,
                plan,
                activeMapping,
                currentFrequency,
                curMode);

            // Source and target may be the same band, so reload after
            // write-back before promoting a populated target row.
            registers = readRegisters(mem, plan, mapping);
        }
        std::optional<BandRegister> selected;
        if (frozenTop) {
            // Slot 0 is a frozen, already-persisted popup checkpoint. A newer
            // state sampled above may have temporarily updated the same band's
            // top; restore the value the user actually selected before recall.
            registers.storeTop(*frozenTop);
            selected = frozenTop;
        }
        else {
            // For an empty row in the active target, materialize and promote
            // the current VFO directly. The pre-write stack retained above
            // preserves its old top instead of manufacturing a duplicate VFO.
            selected = registers.select(
                (std::size_t)index,
                materialization);
        }
        assert(selected);
        if (!selected) { return BandRecallResult::NoTarget; }
        target = *selected;
        writeRegisters(mem, mapping, registers);
        band.set(freq_memory::PREFERRED_SERVICE,
                 freq_input::bandServiceKey(mapping.service));
        root.set(freq_memory::SELECTOR, freq_memory::SELECTOR_BAND);
    }

    applyTarget(mapping, target);
    return BandRecallResult::Recalled;
}

void BandStack::commitCurrentBand() {
    // Could be -1 if current VFO is a decoder or there is no VFO active.
    const int mode = currentMode();
    const auto currentFrequency = double(gui::freqSelect.frequency);
    auto configAccess = core::configManager.edit();
    ConfigManager::EditSection root = freq_memory::root(configAccess);
    ConfigManager::EditSection band = root.section(freq_memory::SELECTOR_BAND);
    ConfigManager::EditSection mem = band.section(freq_memory::STACKING_REGISTERS);
    const freq_input::BandService service =
        freq_input::bandServiceFromKey(
            band.value(
                freq_memory::PREFERRED_SERVICE,
                "other"));
    // Shutdown may search only the current service.
    const bandplan::BandPlan_t* plan = gui::waterfall.bandplan;
    const freq_input::BandMapping* current = resolveBandInServices(
        plan,
        currentFrequency,
        freq_input::BandServiceSet::single(service),
        service);
    if (current) {
        updateTopRegister(mem, plan, *current, currentFrequency, mode);
    }
}

// Tune to a resolved register. Mode and channel spacing belong to the source
// segment containing the target frequency, not to an arbitrary Band_t sharing
// the stable identity.
void BandStack::applyTarget(
    const freq_input::BandMapping& mapping,
    const BandRegister& target)
{
    assert(target.freq > 0.0);
    if (target.freq <= 0.0) { return; }
    const bandplan::BandPlan_t* plan = gui::waterfall.bandplan;
    assert(plan);
    if (!plan) { return; }
    const bandplan::Band_t* segment =
        plan->findMappedSegmentAtFrequency(mapping, target.freq);
    assert(segment);
    if (!segment) { return; }
    applySegmentTarget(*segment, target.freq, target.mode);
}

void BandStack::applySegmentTarget(
    const bandplan::Band_t& segment,
    double freq,
    int mode)
{
    assert(freq > 0.0 && segment.containsFrequency(freq));
    mode = resolvedMode(segment, freq, mode);

    std::string vfoName = gui::waterfall.selectedVFO;
    // RADIO_IFACE_CMD_SET_MODE has no early-out on the radio side: selectDemodByID()
    // rebuilds the whole demodulator chain even for the mode already selected,
    // which is an audible click on every band tap. Only send it on a change.
    if (mode >= 0 && isRadioVFO(vfoName) && mode != currentMode()) {
        core::modComManager.callInterface(vfoName, RADIO_IFACE_CMD_SET_MODE, &mode, NULL);
    }
    // Channelized bands set the VFO snap after the mode change, which would
    // otherwise reset the snap to the mode default.
    if (segment.chan > 0.0 && !vfoName.empty()) {
        auto vit = gui::waterfall.vfos.find(vfoName);
        if (vit != gui::waterfall.vfos.end() && vit->second) {
            vit->second->setSnapInterval(segment.chan);
        }
    }

    // SET_MODE may change the VFO reference, bandwidth and offsets. Tune only
    // after that reconfiguration so the recalled absolute frequency wins.
    tuner::tune(tuner::TUNER_MODE_NORMAL, vfoName, freq);
}

// Resolve current VFO's demodulator mode. Return -1 for non-radio VFO (such as some decoder like Radiosonde)
int BandStack::currentMode() const {
    std::string vfoName = gui::waterfall.selectedVFO;
    if (!isRadioVFO(vfoName)) { return -1; }
    int mode = -1;
    core::modComManager.callInterface(vfoName, RADIO_IFACE_CMD_GET_MODE, NULL, &mode);
    return mode;
}
