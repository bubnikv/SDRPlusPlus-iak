#include <gui/band_stack.h>
#include <gui/widgets/bandplan.h>
#include <gui/gui.h>
#include <config.h>
#include <core.h>
#include <radio_interface.h>
#include <cmath>

// IC-705 parity: three registers per band. The limit is a hardware panel
// constraint rather than an inherent one (see doc/design/band-stack.md), so it
// lives in one place ready to be lifted.
static const size_t MAX_REGISTERS = 3;

static bool isRadioVFO(const std::string& vfoName) {
    return !vfoName.empty() && core::modComManager.interfaceExists(vfoName)
        && core::modComManager.getModuleName(vfoName) == "radio";
}

static std::string bandMemoryKey(const bandplan::Band_t& band) {
    return band.bandId;
}

// A stable band may be represented by several disjoint or adjacent segments in
// a legacy plan. Revalidate a register against the union of those segments,
// rather than only the selector row through which the register was opened.
static bool frequencyBelongsToBand(const bandplan::Band_t& band, double freq) {
    if (band.bandId.empty()) {
        return false;
    }
    const bandplan::BandPlan_t* plan = gui::waterfall.bandplan;
    if (!plan) { return false; }
    for (const auto& candidate : plan->bands) {
        if (candidate.bandId == band.bandId &&
            freq >= candidate.start && freq <= candidate.end)
        {
            return true;
        }
    }
    return false;
}

// A band's stored registers, newest first, tolerating the legacy single-object
// format and dropping any entry no longer inside the (possibly changed) band
// edges -- the same containment revalidation the single-slot design used.
static std::vector<BandRegister> readRegisters(const json& mem, const bandplan::Band_t& band) {
    std::vector<BandRegister> regs;
    if (band.bandId.empty() || !mem.is_object()) { return regs; }
    auto it = mem.find(bandMemoryKey(band));
    // Before stable IDs, band memory was keyed by the display name. Read it as
    // a fallback so an app update does not discard the user's last frequencies.
    if (it == mem.end() && !band.bandId.empty()) { it = mem.find(band.name); }
    if (it == mem.end()) { return regs; }
    auto pushOne = [&](const json& o) {
        if (!o.is_object()) { return; }
        double f = o.value("freq", 0.0);
        if (!frequencyBelongsToBand(band, f)) { return; }
        int m = o.value("mode", -1);
        if (m < 0 || m >= _RADIO_IFACE_MODE_COUNT) { m = -1; }
        regs.push_back({ f, m });
    };
    if (it->is_array()) {
        for (const auto& e : *it) {
            pushOne(e);
            if (regs.size() >= MAX_REGISTERS) { break; }
        }
    }
    else {
        pushOne(*it); // legacy single register
    }
    return regs;
}

// Push freq/mode to the front of a band's register stack: newest first, deduped
// by frequency, capped at MAX_REGISTERS. IC-705: "when you change the operating
// band or the Register, the previously operated frequency and mode are stored."
static void pushRegister(
    json& mem,
    const bandplan::Band_t& band,
    double freq,
    int mode)
{
    if (band.bandId.empty()) { return; }
    if (!mem.is_object()) { mem = json::object(); }
    const std::string key = bandMemoryKey(band);
    json existing = json::array();
    auto it = mem.find(key);
    if (it == mem.end() && !band.bandId.empty()) { it = mem.find(band.name); }
    if (it != mem.end()) {
        if (it->is_array()) { existing = *it; }
        else if (it->is_object()) { existing.push_back(*it); } // migrate legacy
    }
    json out = json::array();
    out.push_back({ { "freq", freq }, { "mode", mode } });
    for (const auto& e : existing) {
        if (out.size() >= MAX_REGISTERS) { break; }
        if (!e.is_object()) { continue; }
        if (std::abs(e.value("freq", 0.0) - freq) < 1.0) { continue; } // dedup same frequency
        out.push_back(e);
    }
    mem[key] = out;
    if (band.name != key) { mem.erase(band.name); }
}

// Resolve only inside the last active service. Preserve its explicitly selected
// band while it remains valid; otherwise accept only one distinct stable ID.
// This avoids manufacturing an answer from JSON order when bands overlap.
static const bandplan::Band_t* resolveActiveBand(
    const bandplan::BandPlan_t* plan,
    double freq,
    freq_input::BandService service,
    const std::string& preferredBandId)
{
    // An empty preferred ID means the last explicitly selected row had no
    // stable identity. Stacking is disabled for that selection; do not infer a
    // different row merely because its span overlaps the tuned frequency.
    if (!plan || service == freq_input::BandService::Other ||
        preferredBandId.empty())
    {
        return nullptr;
    }

    const bandplan::Band_t* preferred = nullptr;
    const bandplan::Band_t* unique = nullptr;
    std::string uniqueId;
    bool ambiguous = false;
    for (const auto& band : plan->bands) {
        if (band.bandId.empty() ||
            (band.entityKind != freq_input::LegacyEntityKind::Band &&
             band.entityKind != freq_input::LegacyEntityKind::Segment) ||
            band.service != service ||
            freq < band.start || freq > band.end)
        {
            continue;
        }

        if (!preferredBandId.empty() && band.bandId == preferredBandId &&
            (!preferred || (band.end - band.start) < (preferred->end - preferred->start)))
        {
            preferred = &band;
        }

        if (uniqueId.empty()) {
            uniqueId = band.bandId;
            unique = &band;
        }
        else if (band.bandId == uniqueId) {
            if ((band.end - band.start) < (unique->end - unique->start)) {
                unique = &band;
            }
        }
        else {
            ambiguous = true;
        }
    }

    if (preferred) { return preferred; }
    return ambiguous ? nullptr : unique;
}

// Write back the active stable band. The caller holds the config lock.
static const bandplan::Band_t* storeCurrentBand(
    json& mem,
    int mode,
    freq_input::BandService lastActiveService,
    const std::string& preferredBandId)
{
    const bandplan::BandPlan_t* plan = gui::waterfall.bandplan;
    if (!plan) { return nullptr; }
    double freq = (double)gui::freqSelect.frequency;
    const bandplan::Band_t* best =
        resolveActiveBand(plan, freq, lastActiveService, preferredBandId);
    if (best) { pushRegister(mem, *best, freq, mode); }
    return best;
}

std::vector<BandRegister> BandStack::registersFor(const bandplan::Band_t& band) const {
    std::vector<BandRegister> regs;
    if (band.bandId.empty()) { return regs; }
    core::configManager.acquire();
    // Bound as const so a band never visited isn't given a null entry in the
    // config as a side effect of being looked up.
    const json& conf = core::configManager.conf;
    auto it = conf.find("bandMemory");
    if (it != conf.end()) { regs = readRegisters(*it, band); }
    core::configManager.release();
    return regs;
}

std::string BandStack::activeBandId(double frequency) const {
    core::configManager.acquire();
    const json& conf = core::configManager.conf;
    const freq_input::BandService service =
        freq_input::bandServiceFromKey(
            conf.value("lastActiveBandService", "other"));
    const std::string preferredBandId =
        conf.value("lastActiveBandId", std::string());
    core::configManager.release();

    const bandplan::Band_t* band =
        resolveActiveBand(gui::waterfall.bandplan, frequency, service, preferredBandId);
    return band ? band->bandId : std::string();
}

// Service/frequency mode convention applied when a band carries no def_mode.
// Keep in sync with heuristic_mode() in scripts/enrich_bandplans.py.
int BandStack::heuristicMode(const bandplan::Band_t& b) {
    if (b.service == freq_input::BandService::Amateur) {
        if (b.end <= 600000.0) { return RADIO_IFACE_MODE_CW; }                             // 2200 m / 630 m
        if (b.start >= 5200000.0 && b.start <= 5500000.0) { return RADIO_IFACE_MODE_USB; } // 60 m channels
        if (b.start < 10000000.0) { return RADIO_IFACE_MODE_LSB; }
        if (b.start < 100000000.0) { return RADIO_IFACE_MODE_USB; }                        // 30 m .. 6 m/4 m
        return RADIO_IFACE_MODE_NFM;                                                       // 2 m and up: repeaters
    }
    if (b.service == freq_input::BandService::Broadcast) {
        if (b.family == freq_input::BandFamily::WeatherBroadcast) {
            return RADIO_IFACE_MODE_NFM;
        }
        if (b.family == freq_input::BandFamily::TelevisionBroadcast) {
            return -1;
        }
        return (b.start >= 30000000.0) ? RADIO_IFACE_MODE_WFM : RADIO_IFACE_MODE_AM;
    }
    if (b.service == freq_input::BandService::Aviation) {
        if (b.family == freq_input::BandFamily::AviationSurveillance) {
            return -1;
        }
        return (b.start < 30000000.0) ? RADIO_IFACE_MODE_USB : RADIO_IFACE_MODE_AM;
    }
    if (b.service == freq_input::BandService::Maritime) {
        return (b.start < 30000000.0) ? RADIO_IFACE_MODE_USB : RADIO_IFACE_MODE_NFM;
    }
    return -1; // no mode change for other band types
}

void BandStack::selectBand(const bandplan::Band_t& band) {
    int curMode = currentMode();
    double targetFreq = 0.0;
    int targetMode = -1;

    core::configManager.acquire();
    json& conf = core::configManager.conf;
    json& mem = conf["bandMemory"];
    const freq_input::BandService previousService =
        freq_input::bandServiceFromKey(
            conf.value("lastActiveBandService", "other"));
    const std::string previousBandId =
        conf.value("lastActiveBandId", std::string());
    storeCurrentBand(mem, curMode, previousService, previousBandId);
    conf["lastActiveBandService"] =
        std::string(freq_input::bandServiceKey(band.service));
    conf["lastActiveBandId"] = band.bandId;
    // Read after the write-back: tapping the band already tuned to promotes the
    // current frequency to register 1, so the recall below is a no-op.
    std::vector<BandRegister> regs = readRegisters(mem, band);
    core::configManager.release(true);

    if (!regs.empty()) {
        targetFreq = regs[0].freq;
        targetMode = regs[0].mode;
    }
    applyTarget(band, targetFreq, targetMode);
}

void BandStack::recallRegister(const bandplan::Band_t& band, int index) {
    int curMode = currentMode();
    double targetFreq = 0.0;
    int targetMode = -1;

    core::configManager.acquire();
    json& conf = core::configManager.conf;
    json& mem = conf["bandMemory"];
    // Resolve the pick against the list as the user saw it, before the
    // write-back below can reorder this band's own stack -- unlike selectBand,
    // where reading afterwards is what makes a repeat tap a no-op.
    std::vector<BandRegister> regs = readRegisters(mem, band);
    if (index >= 0 && index < (int)regs.size()) {
        targetFreq = regs[index].freq;
        targetMode = regs[index].mode;
    }
    const freq_input::BandService previousService =
        freq_input::bandServiceFromKey(
            conf.value("lastActiveBandService", "other"));
    const std::string previousBandId =
        conf.value("lastActiveBandId", std::string());
    storeCurrentBand(mem, curMode, previousService, previousBandId);
    conf["lastActiveBandService"] =
        std::string(freq_input::bandServiceKey(band.service));
    conf["lastActiveBandId"] = band.bandId;
    core::configManager.release(true);

    applyTarget(band, targetFreq, targetMode);
}

void BandStack::commitCurrent() {
    const int mode = currentMode();
    core::configManager.acquire();
    json& conf = core::configManager.conf;
    json& mem = conf["bandMemory"];
    const freq_input::BandService service =
        freq_input::bandServiceFromKey(
            conf.value("lastActiveBandService", "other"));
    const std::string preferredBandId =
        conf.value("lastActiveBandId", std::string());
    const bandplan::Band_t* current =
        storeCurrentBand(mem, mode, service, preferredBandId);
    conf["lastActiveBandId"] =
        current ? current->bandId : std::string();
    core::configManager.release(true);
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
