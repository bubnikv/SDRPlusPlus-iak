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

// A band's stored registers, newest first, tolerating the legacy single-object
// format and dropping any entry no longer inside the (possibly changed) band
// edges -- the same containment revalidation the single-slot design used.
static std::vector<BandRegister> readRegisters(const json& mem, const bandplan::Band_t& band) {
    std::vector<BandRegister> regs;
    auto it = mem.find(band.name);
    if (it == mem.end()) { return regs; }
    auto pushOne = [&](const json& o) {
        if (!o.is_object()) { return; }
        double f = o.value("freq", 0.0);
        if (f < band.start || f > band.end) { return; }
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
static void pushRegister(json& mem, const std::string& name, double freq, int mode) {
    json existing = json::array();
    auto it = mem.find(name);
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
    mem[name] = out;
}

// Write-back for the band currently being left: the first band of the selected
// plan containing the current frequency. Names repeat across plans (and within:
// e.g. "Shortwave Broadcast" segments), so the first containing band wins and
// reads are revalidated by containment. The caller holds the config lock.
static void storeCurrentBand(json& mem, int mode) {
    const bandplan::BandPlan_t* plan = gui::waterfall.bandplan;
    if (!plan) { return; }
    double freq = (double)gui::freqSelect.frequency;
    for (const auto& b : plan->bands) {
        if (freq >= b.start && freq <= b.end) {
            pushRegister(mem, b.name, freq, mode);
            break;
        }
    }
}

std::vector<BandRegister> BandStack::registersFor(const bandplan::Band_t& band) const {
    std::vector<BandRegister> regs;
    core::configManager.acquire();
    // Bound as const so a band never visited isn't given a null entry in the
    // config as a side effect of being looked up.
    const json& conf = core::configManager.conf;
    auto it = conf.find("bandMemory");
    if (it != conf.end()) { regs = readRegisters(*it, band); }
    core::configManager.release();
    return regs;
}

// Band-type/frequency mode convention applied when a band carries no def_mode.
// Keep in sync with heuristic_mode() in scripts/enrich_bandplans.py.
int BandStack::heuristicMode(const bandplan::Band_t& b) {
    if (b.type == "amateur" || b.type == "amateur1") {
        if (b.end <= 600000.0) { return RADIO_IFACE_MODE_CW; }                             // 2200 m / 630 m
        if (b.start >= 5200000.0 && b.start <= 5500000.0) { return RADIO_IFACE_MODE_USB; } // 60 m channels
        if (b.start < 10000000.0) { return RADIO_IFACE_MODE_LSB; }
        if (b.start < 100000000.0) { return RADIO_IFACE_MODE_USB; }                        // 30 m .. 6 m/4 m
        return RADIO_IFACE_MODE_NFM;                                                       // 2 m and up: repeaters
    }
    if (b.type == "broadcast") {
        return (b.start >= 30000000.0) ? RADIO_IFACE_MODE_WFM : RADIO_IFACE_MODE_AM;
    }
    if (b.type == "aviation" || b.type == "aircraft") {
        return (b.start < 30000000.0) ? RADIO_IFACE_MODE_USB : RADIO_IFACE_MODE_AM;
    }
    if (b.type == "marine" || b.type == "marine1") {
        return (b.start < 30000000.0) ? RADIO_IFACE_MODE_USB : RADIO_IFACE_MODE_NFM;
    }
    return -1; // no mode change for other band types
}

void BandStack::selectBand(const bandplan::Band_t& band) {
    int curMode = currentMode();
    double targetFreq = 0.0;
    int targetMode = -1;

    core::configManager.acquire();
    json& mem = core::configManager.conf["bandMemory"];
    storeCurrentBand(mem, curMode);
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
    json& mem = core::configManager.conf["bandMemory"];
    // Resolve the pick against the list as the user saw it, before the
    // write-back below can reorder this band's own stack -- unlike selectBand,
    // where reading afterwards is what makes a repeat tap a no-op.
    std::vector<BandRegister> regs = readRegisters(mem, band);
    if (index >= 0 && index < (int)regs.size()) {
        targetFreq = regs[index].freq;
        targetMode = regs[index].mode;
    }
    storeCurrentBand(mem, curMode);
    core::configManager.release(true);

    applyTarget(band, targetFreq, targetMode);
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
