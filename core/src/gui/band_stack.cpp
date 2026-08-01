#include <gui/band_stack.h>
#include <gui/widgets/bandplan.h>
#include <gui/widgets/freq_input/spectrum_ranges.h>
#include <gui/gui.h>
#include <config.h>
#include <core.h>
#include <radio_interface.h>
#include <algorithm>
#include <cmath>

// IC-705 parity and UI contract: every band has three rotating optional
// entries, with entry 0 always current.
static const size_t MAX_REGISTERS = 3;

// for getting / setting radio mode
//FIXME we may consider adding mode identifiers to demodulators such as USB for FT8.
static bool isRadioVFO(const std::string& vfoName) {
    return !vfoName.empty() && core::modComManager.interfaceExists(vfoName)
        && core::modComManager.getModuleName(vfoName) == "radio";
}

// A stable band may be represented by several disjoint or adjacent segments in
// a legacy plan. Revalidate a register against the union of those segments,
// rather than only the selector row through which the register was opened.
static bool frequencyBelongsToBand(const bandplan::Band_t& band, double freq) {
    if (!band.resolved.mapping) {
        return false;
    }
    const bandplan::BandPlan_t* plan = gui::waterfall.bandplan;
    if (!plan) { return false; }
    for (const auto& candidate : plan->bands) {
        if (candidate.resolved.mapping == band.resolved.mapping &&
            freq >= candidate.start && freq <= candidate.end)
        {
            return true;
        }
    }
    return false;
}

// A band's three rotating optional slots. Entry 0 is always current. Invalid
// persisted entries become empty; there is no public-release migration path.
static std::vector<BandRegister> readRegisters(const json& mem, const bandplan::Band_t& band) {
    std::vector<BandRegister> regs(MAX_REGISTERS);
    if (!band.resolved.mapping || !mem.is_object()) { return regs; }
    auto it = mem.find(std::string(band.resolved.bandId()));
    if (it == mem.end() || !it->is_array()) { return regs; }
    for (std::size_t i = 0; i < MAX_REGISTERS && i < it->size(); i++) {
        const json& o = (*it)[i];
        if (!o.is_object()) { continue; }
        double f = o.value("freq", 0.0);
        if (!frequencyBelongsToBand(band, f)) { continue; }
        int m = o.value("mode", -1);
        if (m < 0 || m >= _RADIO_IFACE_MODE_COUNT) { m = -1; }
        regs[i] = { true, f, m };
    }
    return regs;
}

static void writeRegisters(
    json& mem,
    const bandplan::Band_t& band,
    const std::vector<BandRegister>& registers)
{
    if (!band.resolved.mapping) { return; }
    if (!mem.is_object()) { mem = json::object(); }
    const std::string key(band.resolved.bandId());
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

static bool updateTopRegister(
    json& mem,
    const bandplan::Band_t& band,
    double freq,
    int mode)
{
    if (!frequencyBelongsToBand(band, freq)) { return false; }
    std::vector<BandRegister> registers = readRegisters(mem, band);
    registers[0] = { true, freq, mode };
    writeRegisters(mem, band, registers);
    return true;
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

struct BandCandidate {
    const bandplan::Band_t* band = nullptr;
    const freq_input::BandMapping* mapping = nullptr;
};

// Resolve one stable band inside a UI group. Visible resolution may fall back
// to another service; lifecycle resolution sets lockService and never does.
static const bandplan::Band_t* resolveBandInGroup(
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

    std::vector<BandCandidate> candidates;
    for (const auto& band : plan->bands) {
        const freq_input::BandMapping* mapping = band.resolved.mapping;
        if (!mapping ||
            (band.resolved.entityKind() !=
                 freq_input::LegacyEntityKind::Band &&
             band.resolved.entityKind() !=
                 freq_input::LegacyEntityKind::Segment) ||
            !serviceInGroup(band.resolved.service(), group) ||
            (lockService && band.resolved.service() != preferredService) ||
            freq < band.start || freq > band.end)
        {
            continue;
        }

        auto existing = std::find_if(
            candidates.begin(),
            candidates.end(),
            [&](const BandCandidate& candidate) {
                return candidate.mapping == mapping;
            });
        if (existing == candidates.end()) {
            candidates.push_back({ &band, mapping });
        }
        else if ((band.end - band.start) <
                 (existing->band->end - existing->band->start))
        {
            existing->band = &band;
        }
    }

    for (const BandCandidate& candidate : candidates) {
        if (!preferredBandId.empty() &&
            candidate.mapping->bandId == std::string_view(
                preferredBandId.data(),
                preferredBandId.size()))
        {
            return candidate.band;
        }
    }

    const bandplan::Band_t* serviceMatch = nullptr;
    int serviceMatches = 0;
    if (preferredService != freq_input::BandService::Other) {
        for (const BandCandidate& candidate : candidates) {
            if (candidate.mapping->service == preferredService) {
                serviceMatch = candidate.band;
                serviceMatches++;
            }
        }
        if (serviceMatches == 1) { return serviceMatch; }
        if (serviceMatches > 1) { return nullptr; }
    }

    return candidates.size() == 1 ? candidates[0].band : nullptr;
}

static const bandplan::Band_t* bandById(const std::string& bandId) {
    const bandplan::BandPlan_t* plan = gui::waterfall.bandplan;
    if (!plan || bandId.empty()) { return nullptr; }
    const std::string_view requestedId(bandId.data(), bandId.size());
    for (const auto& band : plan->bands) {
        if (band.resolved.mapping &&
            band.resolved.mapping->bandId == requestedId &&
            (band.resolved.entityKind() ==
                 freq_input::LegacyEntityKind::Band ||
             band.resolved.entityKind() ==
                 freq_input::LegacyEntityKind::Segment))
        {
            return &band;
        }
    }
    return nullptr;
}

// Write only to the stable ID the UI visibly selected. No frequency search is
// allowed on an explicit band/register transition.
static bool storeVisibleBand(
    json& mem,
    const std::string& activeBandId,
    double frequency,
    int mode)
{
    const bandplan::Band_t* band = bandById(activeBandId);
    if (!band || !updateTopRegister(mem, *band, frequency, mode)) {
        return false;
    }
    return true;
}

std::vector<BandRegister> BandStack::registersFor(const bandplan::Band_t& band) const {
    std::vector<BandRegister> regs(MAX_REGISTERS);
    if (!band.resolved.mapping) { return regs; }
    core::configManager.acquire();
    // Bound as const so a band never visited isn't given a null entry in the
    // config as a side effect of being looked up.
    const json& conf = core::configManager.conf;
    auto it = conf.find("bandMemory");
    if (it != conf.end()) { regs = readRegisters(*it, band); }
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

    const bandplan::Band_t* active = resolveBandInGroup(
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
            freq_input::bandServiceKey(active->resolved.service()));
        const std::string bandId(active->resolved.bandId());
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
    return active ? std::string(active->resolved.bandId()) : std::string();
}

// Service/frequency mode convention applied when a band carries no def_mode.
// Keep in sync with heuristic_mode() in scripts/enrich_bandplans.py.
int BandStack::heuristicMode(const bandplan::Band_t& b) {
    if (b.resolved.service() == freq_input::BandService::Amateur) {
        if (b.end <= 600000.0) { return RADIO_IFACE_MODE_CW; }                             // 2200 m / 630 m
        if (b.start >= 5200000.0 && b.start <= 5500000.0) { return RADIO_IFACE_MODE_USB; } // 60 m channels
        if (b.start < 10000000.0) { return RADIO_IFACE_MODE_LSB; }
        if (b.start < 100000000.0) { return RADIO_IFACE_MODE_USB; }                        // 30 m .. 6 m/4 m
        return RADIO_IFACE_MODE_NFM;                                                       // 2 m and up: repeaters
    }
    if (b.resolved.service() == freq_input::BandService::Broadcast) {
        if (b.resolved.family() == freq_input::BandFamily::WeatherBroadcast) {
            return RADIO_IFACE_MODE_NFM;
        }
        if (b.resolved.family() == freq_input::BandFamily::TelevisionBroadcast) {
            return -1;
        }
        return (b.start >= 30000000.0) ? RADIO_IFACE_MODE_WFM : RADIO_IFACE_MODE_AM;
    }
    if (b.resolved.service() == freq_input::BandService::Aviation) {
        if (b.resolved.family() == freq_input::BandFamily::AviationSurveillance) {
            return -1;
        }
        return (b.start < 30000000.0) ? RADIO_IFACE_MODE_USB : RADIO_IFACE_MODE_AM;
    }
    if (b.resolved.service() == freq_input::BandService::Maritime) {
        return (b.start < 30000000.0) ? RADIO_IFACE_MODE_USB : RADIO_IFACE_MODE_NFM;
    }
    return -1; // no mode change for other band types
}

void BandStack::selectBand(
    const bandplan::Band_t& band,
    const std::string& activeBandId,
    const std::string& group)
{
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
        activeBandId,
        currentFrequency,
        curMode);
    const std::string bandId(band.resolved.bandId());
    const bool repeatTap =
        storedSource && !bandId.empty() && activeBandId == bandId;

    conf["lastActiveBandService"] =
        std::string(freq_input::bandServiceKey(band.resolved.service()));
    conf["lastActiveBandId"] = bandId;
    conf["freqEntryCategory"] = group;
    conf["lastMemorySelector"] = "band";

    std::vector<BandRegister> registers = readRegisters(mem, band);
    if (repeatTap) {
        std::rotate(
            registers.begin(),
            registers.begin() + 1,
            registers.end());
        writeRegisters(mem, band, registers);
    }

    if (!registers.empty() && registers[0].populated) {
        targetFreq = registers[0].freq;
        targetMode = registers[0].mode;
        applyStoredTarget = true;
    }
    else if (!repeatTap) {
        applyDefaultTarget = true;
    }
    core::configManager.release(true);

    if (applyStoredTarget) {
        applyTarget(band, targetFreq, targetMode);
    }
    else if (applyDefaultTarget) {
        applyTarget(band, 0.0, -1);
    }
}

void BandStack::recallRegister(
    const bandplan::Band_t& band,
    int index,
    const std::string& activeBandId,
    const std::string& group)
{
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
        activeBandId,
        currentFrequency,
        curMode);

    std::vector<BandRegister> registers = readRegisters(mem, band);
    if (index >= 0 && index < (int)registers.size()) {
        std::rotate(
            registers.begin(),
            registers.begin() + index,
            registers.end());
        writeRegisters(mem, band, registers);
        if (registers[0].populated) {
            targetFreq = registers[0].freq;
            targetMode = registers[0].mode;
            applyStoredTarget = true;
        }
    }
    conf["lastActiveBandService"] =
        std::string(freq_input::bandServiceKey(band.resolved.service()));
    conf["lastActiveBandId"] = std::string(band.resolved.bandId());
    conf["freqEntryCategory"] = group;
    conf["lastMemorySelector"] = "band";
    core::configManager.release(true);

    if (applyStoredTarget) {
        applyTarget(band, targetFreq, targetMode);
    }
}

void BandStack::commitCurrent() {
    const int mode = currentMode();
    const double currentFrequency = (double)gui::freqSelect.frequency;
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
    const std::string group =
        conf.value("freqEntryCategory", "Ham");
    const bandplan::Band_t* current = resolveBandInGroup(
        gui::waterfall.bandplan,
        currentFrequency,
        group,
        service,
        preferredBandId,
        true);
    if (current &&
        updateTopRegister(mem, *current, currentFrequency, mode))
    {
        // The stable band may follow manual tuning inside the current service;
        // the service itself is deliberately never changed on save.
        conf["lastActiveBandId"] = std::string(current->resolved.bandId());
        core::configManager.release(true);
    }
    else {
        core::configManager.release(false);
    }
}

// Tune to a resolved register, filling in the band's defaults for anything the
// register did not carry.
void BandStack::applyTarget(const bandplan::Band_t& band, double freq, int mode) {
    if (freq <= 0.0) {
        freq = (band.defFreq > 0.0) ? band.defFreq
                                    : round((band.start + band.end) / 2.0 / 1000.0) * 1000.0;
    }
    if (mode < 0) {
        mode = radioModeFromName(band.defMode.c_str());
        if (mode < 0) { mode = heuristicMode(band); }
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
    if (band.chan > 0.0 && !vfoName.empty()) {
        auto vit = gui::waterfall.vfos.find(vfoName);
        if (vit != gui::waterfall.vfos.end() && vit->second) { vit->second->setSnapInterval(band.chan); }
    }
}

// The application's "a tune was requested by the UI" mailbox: MainWindow::draw()
// picks the flag up, calls tuner::tune() and persists the frequency. Going
// straight to tuner::tune() from here would skip that bookkeeping.
void BandStack::requestTune(double freq) {
    gui::freqSelect.setFrequency((int64_t)round(freq));
    gui::freqSelect.frequencyChanged = true;
}

int BandStack::currentMode() const {
    std::string vfoName = gui::waterfall.selectedVFO;
    if (!isRadioVFO(vfoName)) { return -1; }
    int mode = -1;
    core::modComManager.callInterface(vfoName, RADIO_IFACE_CMD_GET_MODE, NULL, &mode);
    return mode;
}
