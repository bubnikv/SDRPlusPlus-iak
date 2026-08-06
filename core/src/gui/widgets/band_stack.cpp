#include <gui/widgets/band_stack.h>
#include <gui/widgets/bandplan.h>
#include <gui/widgets/freq_memory.h>
#include <gui/widgets/freq_input/spectrum_ranges.h>
#include <gui/gui.h>
#include <gui/tuner.h>
#include <config.h>
#include <core.h>
#include <radio_interface.h>
#include <algorithm>
#include <cassert>
#include <cmath>

namespace gui {
    BandStack bandStack;
}

// IC-705 parity and UI contract: every band has three rotating optional
// entries, with entry 0 always current.
static const size_t MAX_REGISTERS = 3;

// for getting / setting radio mode
//FIXME we may consider adding mode identifiers to demodulators such as USB for FT8.
static bool isRadioVFO(const std::string& vfoName) {
    return !vfoName.empty() && core::modComManager.interfaceExists(vfoName)
        && core::modComManager.getModuleName(vfoName) == "radio";
}

// One stable band_id's three rotating optional slots. Entry 0 is always
// current. Invalid persisted entries become empty; there is no public-release
// migration path.
static std::vector<BandRegister> readRegisters(
    const ConfigManager::ReadSection& mem,
    const bandplan::BandPlan_t* plan,
    const freq_input::BandMapping& mapping)
{
    std::vector<BandRegister> regs(MAX_REGISTERS);
    const json* entries = mem.section(mapping.bandId).peek();
    if (!entries || !entries->is_array()) { return regs; }
    for (std::size_t i = 0; i < MAX_REGISTERS && i < entries->size(); i++) {
        const json& o = (*entries)[i];
        if (!o.is_object()) { continue; }
        double f = o.value("freq", 0.0);
        if (f <= 0.0 || !plan ||
            !plan->findMappedSegmentAtFrequency(mapping, f))
        {
            continue;
        }
        int m = o.value("mode", -1);
        if (m < 0 || m >= _RADIO_IFACE_MODE_COUNT) { m = -1; }
        regs[i] = { true, f, m };
    }
    return regs;
}

// Returns whether the stored entries actually differ from what's already there,
// so re-tuning to the frequency a band is already parked on doesn't dirty the
// config.
static bool writeRegisters(
    ConfigManager::EditSection mem,
    const freq_input::BandMapping& mapping,
    const std::vector<BandRegister>& registers)
{
    json out = json::array();
    for (std::size_t i = 0; i < MAX_REGISTERS; i++) {
        if (i >= registers.size() || !registers[i].populated) {
            out.push_back(nullptr);
        }
        else {
            out.push_back({
                { "freq", registers[i].freq },
                { "mode", registers[i].mode }
            });
        }
    }
    return mem.set(mapping.bandId, out);
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
    std::vector<BandRegister> registers = readRegisters(mem, plan, mapping);
    registers[0] = { true, freq, mode };
    writeRegisters(mem, mapping, registers);
    return true;
}

// Write only to the stable ID the UI visibly selected. Membership checks its
// segment union; they must never search for a different ID opportunistically.
static bool storeVisibleBand(
    ConfigManager::EditSection mem,
    const bandplan::BandPlan_t* plan,
    const std::string& activeBandId,
    double frequency,
    int mode)
{
    const freq_input::BandMapping* mapping =
        freq_input::findBandMappingById(activeBandId);
    return mapping &&
        updateTopRegister(mem, plan, *mapping, frequency, mode);
}

std::vector<BandRegister> BandStack::registersFor(
    std::string_view bandId) const
{
    const freq_input::BandMapping* mapping =
        freq_input::findBandMappingById(bandId);
    const bandplan::BandPlan_t* plan = gui::waterfall.bandplan;
    if (!mapping || !plan) { return std::vector<BandRegister>(MAX_REGISTERS); }
    // Reads resolve the path without creating it, so a band never visited isn't
    // given an entry in the config as a side effect of being looked up.
    auto configAccess = core::configManager.read();
    return readRegisters(freq_memory::stackingRegisters(configAccess), plan, *mapping);
}

std::string BandStack::activateBandForServices(
    freq_input::BandServiceSet services,
    freq_input::BandService currentService,
    double frequency)
{
    const freq_input::BandMapping* active = resolveBandInServices(
        gui::waterfall.bandplan,
        frequency,
        services,
        currentService);

    auto configAccess = core::configManager.edit();
    freq_memory::root(configAccess).set(freq_memory::SELECTOR, freq_memory::SELECTOR_BAND);
    if (active) {
        ConfigManager::EditSection band = freq_memory::band(configAccess);
        band.set(freq_memory::ACTIVE_SERVICE,
                 freq_input::bandServiceKey(active->service));
        band.set(freq_memory::ACTIVE_ID, active->bandId);
    }
    return active ? std::string(active->bandId) : std::string();
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

void BandStack::selectLegacySegment(
    const bandplan::Band_t& segment,
    const std::string& activeBandId)
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
            activeBandId,
            currentFrequency,
            curMode);
        ConfigManager::EditSection band = freq_memory::band(configAccess);
        band.set(freq_memory::ACTIVE_SERVICE, freq_input::bandServiceKey(segment.resolved.service()));
        band.set(freq_memory::ACTIVE_ID, "");
        freq_memory::root(configAccess).set(freq_memory::SELECTOR, freq_memory::SELECTOR_BAND);
    }

    applySegmentTarget(segment, targetFrequency, -1);
}

void BandStack::selectBand(
    std::string_view bandId,
    double defaultFrequency,
    const std::string& activeBandId)
{
    const bandplan::BandPlan_t    *plan    = gui::waterfall.bandplan;
    const freq_input::BandMapping *mapping = freq_input::findBandMappingById(bandId);
    assert(plan != nullptr);
    assert(mapping != nullptr);
    if (mapping == nullptr)
        return;
    assert(bandId == mapping->bandId);
    const int                      curMode = currentMode();
    const double                   currentFrequency = (double)gui::freqSelect.frequency;
    double                         targetFreq = 0.0;
    int                            targetMode = -1;
    bool                           apply = false;

    {
        auto configAccess = core::configManager.edit();
        ConfigManager::EditSection mem = freq_memory::stackingRegisters(configAccess);
        const bool storedSource = storeVisibleBand(
            mem,
            plan,
            activeBandId,
            currentFrequency,
            curMode);
        const bool repeatTap = storedSource && activeBandId == bandId;

        ConfigManager::EditSection band = freq_memory::band(configAccess);
        band.set(freq_memory::ACTIVE_SERVICE, freq_input::bandServiceKey(mapping->service));
        band.set(freq_memory::ACTIVE_ID, bandId);
        freq_memory::root(configAccess).set(freq_memory::SELECTOR, freq_memory::SELECTOR_BAND);

        std::vector<BandRegister> registers = readRegisters(mem, plan, *mapping);
        if (repeatTap) {
            std::rotate(
                registers.begin(),
                registers.begin() + 1,
                registers.end());
            writeRegisters(mem, *mapping, registers);
        }

        if (!registers.empty() && registers[0].populated) {
            targetFreq = registers[0].freq;
            targetMode = registers[0].mode;
            apply = true;
        }
        else if (!repeatTap && defaultFrequency > 0.0 &&
                 plan &&
                 plan->findMappedSegmentAtFrequency(*mapping, defaultFrequency))
        {
            targetFreq = defaultFrequency;
            targetMode = -1; // default mode
            apply = true;
        }
    }

    if (apply)
        applyTarget(*mapping, targetFreq, targetMode);
}

void BandStack::recallRegister(
    std::string_view bandId,
    int index,
    const std::string& activeBandId)
{
    const freq_input::BandMapping* mapping =
        freq_input::findBandMappingById(bandId);
    if (!mapping) { return; }
    const bandplan::BandPlan_t* plan = gui::waterfall.bandplan;
    // Could be -1 if current VFO is a decoder or there is no VFO active.
    const int curMode = currentMode();
    const double currentFrequency = (double)gui::freqSelect.frequency;
    double targetFreq = 0.0;
    int targetMode = -1;
    bool applyStoredTarget = false;

    {
        auto configAccess = core::configManager.edit();
        ConfigManager::EditSection mem = freq_memory::stackingRegisters(configAccess);
        storeVisibleBand(
            mem,
            plan,
            activeBandId,
            currentFrequency,
            curMode);

        std::vector<BandRegister> registers = readRegisters(mem, plan, *mapping);
        if (index >= 0 && index < (int)registers.size()) {
            std::rotate(
                registers.begin(),
                registers.begin() + index,
                registers.end());
            if (registers[0].populated) {
                targetFreq = registers[0].freq;
                targetMode = registers[0].mode;
                applyStoredTarget = true;
            }
            else if (currentFrequency > 0.0 && plan &&
                     plan->findMappedSegmentAtFrequency(
                         *mapping,
                         currentFrequency))
            {
                // The selected empty slot is now current. Seed it with the VFO
                // state only when that state belongs to this stable band ID.
                registers[0] = { true, currentFrequency, curMode };
            }
            writeRegisters(mem, *mapping, registers);
        }
        ConfigManager::EditSection band = freq_memory::band(configAccess);
        band.set(freq_memory::ACTIVE_SERVICE,
                 freq_input::bandServiceKey(mapping->service));
        band.set(freq_memory::ACTIVE_ID, mapping->bandId);
        freq_memory::root(configAccess).set(freq_memory::SELECTOR, freq_memory::SELECTOR_BAND);
    }

    if (applyStoredTarget) {
        applyTarget(*mapping, targetFreq, targetMode);
    }
}

void BandStack::commitCurrent() {
    // Could be -1 if current VFO is a decoder or there is no VFO active.
    const int mode = currentMode();
    const auto currentFrequency = double(gui::freqSelect.frequency);
    auto configAccess = core::configManager.edit();

    const std::string selector = freq_memory::root(configAccess).value(
        freq_memory::SELECTOR,
        freq_memory::SELECTOR_BAND);
    if (selector == freq_memory::SELECTOR_SPECTRUM) {
        const freq_input::SpectrumRange* range =
            freq_input::spectrumRangeAtFrequency(
                (std::int64_t)std::llround(currentFrequency));
        if (range) {
            ConfigManager::EditSection spectrum = freq_memory::spectrum(configAccess);
            spectrum.section(freq_memory::LAST_FREQUENCY)
                .set(range->rangeId, currentFrequency);
            spectrum.set(freq_memory::ACTIVE_ID, range->rangeId);
        }
        return;
    }

    ConfigManager::EditSection mem = freq_memory::stackingRegisters(configAccess);
    const freq_input::BandService service =
        freq_input::bandServiceFromKey(
            freq_memory::band(configAccess).value(
                freq_memory::ACTIVE_SERVICE,
                "other"));
    // Shutdown may search only the current service.
    const bandplan::BandPlan_t* plan = gui::waterfall.bandplan;
    const freq_input::BandMapping* current = resolveBandInServices(
        plan,
        currentFrequency,
        freq_input::BandServiceSet::single(service),
        service);
    if (current &&
        updateTopRegister(mem, plan, *current, currentFrequency, mode))
    {
        // The stable band may follow manual tuning inside the current service;
        // the service itself is deliberately never changed on save.
        freq_memory::band(configAccess).set(freq_memory::ACTIVE_ID, current->bandId);
    }
}

// Tune to a resolved register. Mode and channel spacing belong to the source
// segment containing the target frequency, not to an arbitrary Band_t sharing
// the stable identity.
void BandStack::applyTarget(
    const freq_input::BandMapping& mapping,
    double freq,
    int mode)
{
    assert(freq > 0.0);
    const bandplan::BandPlan_t* plan = gui::waterfall.bandplan;
    assert(plan);
    const bandplan::Band_t* segment =
        plan->findMappedSegmentAtFrequency(mapping, freq);
    assert(segment);
    applySegmentTarget(*segment, freq, mode);
}

void BandStack::applySegmentTarget(
    const bandplan::Band_t& segment,
    double freq,
    int mode)
{
    assert(freq > 0.0 && segment.containsFrequency(freq));
    if (mode < 0) {
        mode = radioModeFromName(segment.defMode.c_str());
        if (mode < 0) {
            mode = heuristicMode(
                segment.resolved.service(),
                segment.resolved.family(),
                freq);
        }
    }

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
