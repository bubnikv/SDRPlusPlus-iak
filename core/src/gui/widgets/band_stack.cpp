#include <gui/widgets/band_stack.h>
#include <gui/widgets/bandplan.h>
#include <gui/widgets/freq_input/spectrum_ranges.h>
#include <gui/gui.h>
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
    const json& mem,
    const bandplan::BandPlan_t* plan,
    const freq_input::BandMapping& mapping)
{
    std::vector<BandRegister> regs(MAX_REGISTERS);
    if (!mem.is_object()) { return regs; }
    auto it = mem.find(std::string(mapping.bandId));
    if (it == mem.end() || !it->is_array()) { return regs; }
    for (std::size_t i = 0; i < MAX_REGISTERS && i < it->size(); i++) {
        const json& o = (*it)[i];
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

static void writeRegisters(
    json& mem,
    const freq_input::BandMapping& mapping,
    const std::vector<BandRegister>& registers)
{
    if (!mem.is_object()) { mem = json::object(); }
    const std::string key(mapping.bandId);
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
    mem[key] = out;
}

static bool serviceInGroup(
    freq_input::BandService service,
    const std::string& group)
{
    if (group == "All") { return true; }
    if (group == "Ham") {
        return service == freq_input::BandService::Amateur;
    }
    if (group == "Bcast") {
        return service == freq_input::BandService::Broadcast;
    }
    if (group == "Air") {
        return service == freq_input::BandService::Aviation;
    }
    if (group == "Marine") {
        return service == freq_input::BandService::Maritime;
    }
    if (group == "Util") {
        return service != freq_input::BandService::Amateur &&
            service != freq_input::BandService::Broadcast &&
            service != freq_input::BandService::Aviation &&
            service != freq_input::BandService::Maritime;
    }
    return false;
}

// Resolve one stable band inside a UI group. Visible resolution may fall back
// to another service; lifecycle resolution sets lockService and never does.
static const freq_input::BandMapping* resolveBandInGroup(
    const bandplan::BandPlan_t* plan,
    double freq,
    const std::string& group,
    freq_input::BandService preferredService,
    const std::string& preferredBandId,
    bool lockService)
{
    if (!plan || (lockService &&
        preferredService == freq_input::BandService::Other))
    {
        return nullptr;
    }

    std::vector<const freq_input::BandMapping*> candidates;
    for (const auto& band : plan->bands) {
        const freq_input::BandMapping* mapping = band.resolved.mapping;
        if (!mapping ||
            !band.resolved.isBandOrSegment() ||
            !serviceInGroup(band.resolved.service(), group) ||
            (lockService && band.resolved.service() != preferredService) ||
            !band.containsFrequency(freq))
        {
            continue;
        }

        auto existing = std::find_if(
            candidates.begin(),
            candidates.end(),
            [&](const freq_input::BandMapping* candidate) {
                return candidate == mapping;
            });
        if (existing == candidates.end()) {
            candidates.push_back(mapping);
        }
    }

    for (const freq_input::BandMapping* candidate : candidates) {
        if (!preferredBandId.empty() &&
            candidate->bandId == std::string_view(
                preferredBandId.data(),
                preferredBandId.size()))
        {
            return candidate;
        }
    }

    const freq_input::BandMapping* serviceMatch = nullptr;
    int serviceMatches = 0;
    if (preferredService != freq_input::BandService::Other) {
        for (const freq_input::BandMapping* candidate : candidates) {
            if (candidate->service == preferredService) {
                serviceMatch = candidate;
                serviceMatches++;
            }
        }
        if (serviceMatches == 1) { return serviceMatch; }
        if (serviceMatches > 1) { return nullptr; }
    }

    return candidates.size() == 1 ? candidates[0] : nullptr;
}

static bool updateTopRegister(
    json& mem,
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
    json& mem,
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
    std::vector<BandRegister> regs(MAX_REGISTERS);
    const freq_input::BandMapping* mapping =
        freq_input::findBandMappingById(bandId);
    const bandplan::BandPlan_t* plan = gui::waterfall.bandplan;
    if (!mapping || !plan) { return regs; }
    core::configManager.acquire();
    // Bound as const so a band never visited isn't given a null entry in the
    // config as a side effect of being looked up.
    const json& conf = core::configManager.conf;
    auto it = conf.find("bandMemory");
    if (it != conf.end()) { regs = readRegisters(*it, plan, *mapping); }
    core::configManager.release();
    return regs;
}

std::string BandStack::activateBandForGroup(
    const std::string& group,
    double frequency)
{
    freq_input::BandService preferredService;
    std::string preferredBandId;
    core::configManager.acquire();
    json& conf = core::configManager.conf;
    preferredService = freq_input::bandServiceFromKey(
        conf.value("lastActiveBandService", "other"));
    preferredBandId = conf.value("lastActiveBandId", std::string());
    core::configManager.release();

    const freq_input::BandMapping* active = resolveBandInGroup(
        gui::waterfall.bandplan,
        frequency,
        group,
        preferredService,
        preferredBandId,
        false);

    core::configManager.acquire();
    json& activeConf = core::configManager.conf;
    bool changed = false;
    if (activeConf.value("freqEntryCategory", std::string()) != group) {
        activeConf["freqEntryCategory"] = group;
        changed = true;
    }
    if (activeConf.value("lastMemorySelector", std::string()) != "band") {
        activeConf["lastMemorySelector"] = "band";
        changed = true;
    }
    if (active) {
        const std::string service(
            freq_input::bandServiceKey(active->service));
        const std::string bandId(active->bandId);
        if (activeConf.value("lastActiveBandService", std::string()) != service) {
            activeConf["lastActiveBandService"] = service;
            changed = true;
        }
        if (activeConf.value("lastActiveBandId", std::string()) !=
            bandId)
        {
            activeConf["lastActiveBandId"] = bandId;
            changed = true;
        }
    }
    core::configManager.release(changed);
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
    const std::string& activeBandId,
    const std::string& group)
{
    if (segment.resolved.mapping ||
        !segment.resolved.isBandOrSegment() ||
        !segment.hasValidFrequencySpan() || segment.start < 0.0 ||
        segment.end <= 0.0)
    {
        return;
    }
    const int curMode = currentMode();
    const double currentFrequency = (double)gui::freqSelect.frequency;
    double targetFrequency = segment.defFreq;
    if (targetFrequency <= 0.0 ||
        !segment.containsFrequency(targetFrequency))
    {
        targetFrequency = (segment.start + segment.end) / 2.0;
        const double rounded =
            std::round(targetFrequency / 1000.0) * 1000.0;
        if (rounded > 0.0 && segment.containsFrequency(rounded))
        {
            targetFrequency = rounded;
        }
    }

    core::configManager.acquire();
    json& conf = core::configManager.conf;
    storeVisibleBand(
        conf["bandMemory"],
        gui::waterfall.bandplan,
        activeBandId,
        currentFrequency,
        curMode);
    conf["lastActiveBandService"] = std::string(
        freq_input::bandServiceKey(segment.resolved.service()));
    conf["lastActiveBandId"] = "";
    conf["freqEntryCategory"] = group;
    conf["lastMemorySelector"] = "band";
    core::configManager.release(true);

    applySegmentTarget(segment, targetFrequency, -1);
}

void BandStack::selectBand(
    std::string_view bandId,
    double defaultFrequency,
    const std::string& activeBandId,
    const std::string& group)
{
    const freq_input::BandMapping* mapping =
        freq_input::findBandMappingById(bandId);
    if (!mapping) { return; }
    const bandplan::BandPlan_t* plan = gui::waterfall.bandplan;
    const int curMode = currentMode();
    const double currentFrequency = (double)gui::freqSelect.frequency;
    double targetFreq = 0.0;
    int targetMode = -1;
    bool applyStoredTarget = false;
    bool applyDefaultTarget = false;

    core::configManager.acquire();
    json& conf = core::configManager.conf;
    json& mem = conf["bandMemory"];
    const bool storedSource = storeVisibleBand(
        mem,
        plan,
        activeBandId,
        currentFrequency,
        curMode);
    const std::string targetBandId(mapping->bandId);
    const bool repeatTap =
        storedSource && activeBandId == targetBandId;

    conf["lastActiveBandService"] =
        std::string(freq_input::bandServiceKey(mapping->service));
    conf["lastActiveBandId"] = targetBandId;
    conf["freqEntryCategory"] = group;
    conf["lastMemorySelector"] = "band";

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
        applyStoredTarget = true;
    }
    else if (!repeatTap && defaultFrequency > 0.0 &&
             plan &&
             plan->findMappedSegmentAtFrequency(*mapping, defaultFrequency))
    {
        targetFreq = defaultFrequency;
        applyDefaultTarget = true;
    }
    core::configManager.release(true);

    if (applyStoredTarget) {
        applyTarget(*mapping, targetFreq, targetMode);
    }
    else if (applyDefaultTarget) {
        applyTarget(*mapping, targetFreq, -1);
    }
}

void BandStack::recallRegister(
    std::string_view bandId,
    int index,
    const std::string& activeBandId,
    const std::string& group)
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

    core::configManager.acquire();
    json& conf = core::configManager.conf;
    json& mem = conf["bandMemory"];
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
        writeRegisters(mem, *mapping, registers);
        if (registers[0].populated) {
            targetFreq = registers[0].freq;
            targetMode = registers[0].mode;
            applyStoredTarget = true;
        }
    }
    conf["lastActiveBandService"] =
        std::string(freq_input::bandServiceKey(mapping->service));
    conf["lastActiveBandId"] = std::string(mapping->bandId);
    conf["freqEntryCategory"] = group;
    conf["lastMemorySelector"] = "band";
    core::configManager.release(true);

    if (applyStoredTarget) {
        applyTarget(*mapping, targetFreq, targetMode);
    }
}

void BandStack::commitCurrent() {
    // Could be -1 if current VFO is a decoder or there is no VFO active.
    const int mode = currentMode();
    const auto currentFrequency = double(gui::freqSelect.frequency);
    core::configManager.acquire();
    json& conf = core::configManager.conf;

    if (conf.value("lastMemorySelector", "band") == "spectrum") {
        const freq_input::SpectrumRange* range =
            freq_input::spectrumRangeAtFrequency(
                (std::int64_t)std::llround(currentFrequency));
        if (range) {
            json& memory = conf["spectrumRangeMemory"];
            if (!memory.is_object()) { memory = json::object(); }
            memory[std::string(range->rangeId)] = currentFrequency;
            conf["spectrumLastRangeId"] = std::string(range->rangeId);
            core::configManager.release(true);
        }
        else {
            core::configManager.release(false);
        }
        return;
    }

    json& mem = conf["bandMemory"];
    const freq_input::BandService service =
        freq_input::bandServiceFromKey(
            conf.value("lastActiveBandService", "other"));
    const std::string preferredBandId =
        conf.value("lastActiveBandId", std::string());
    // Shutdown may search only the last visible group and the current service.
    const std::string group =
        conf.value("freqEntryCategory", "Ham");
    const bandplan::BandPlan_t* plan = gui::waterfall.bandplan;
    const freq_input::BandMapping* current = resolveBandInGroup(
        plan,
        currentFrequency,
        group,
        service,
        preferredBandId,
        true);
    if (current &&
        updateTopRegister(mem, plan, *current, currentFrequency, mode))
    {
        // The stable band may follow manual tuning inside the current service;
        // the service itself is deliberately never changed on save.
        conf["lastActiveBandId"] = std::string(current->bandId);
        core::configManager.release(true);
    }
    else {
        core::configManager.release(false);
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
    const bandplan::Band_t* segment = plan
        ? plan->findMappedSegmentAtFrequency(mapping, freq)
        : nullptr;
    if (!segment) { return; }
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

    requestTune(freq);

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
}

// The application's "a tune was requested by the UI" mailbox: MainWindow::draw()
// picks the flag up, calls tuner::tune() and persists the frequency. Going
// straight to tuner::tune() from here would skip that bookkeeping.
void BandStack::requestTune(double freq) {
    gui::freqSelect.setFrequency((int64_t)round(freq));
    gui::freqSelect.frequencyChanged = true;
}

// Resolve current VFO's demodulator mode. Return -1 for non-radio VFO (such as some decoder like Radiosonde)
int BandStack::currentMode() const {
    std::string vfoName = gui::waterfall.selectedVFO;
    if (!isRadioVFO(vfoName)) { return -1; }
    int mode = -1;
    core::modComManager.callInterface(vfoName, RADIO_IFACE_CMD_GET_MODE, NULL, &mode);
    return mode;
}
