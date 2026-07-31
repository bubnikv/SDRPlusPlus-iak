#pragma once
#include <radio_interface.h>
#include <string>
#include <vector>

namespace bandplan {
    struct Band_t;
}

// One band stacking register: a previously operated frequency and mode, in the
// IC-705 sense -- "when you change the operating band or the Register, the
// previously operated frequency and mode are stored". See
// doc/research/band-stacking.md for what the reference implementations keep.
struct BandRegister {
    bool populated = false;
    double freq = 0;  // VFO tune frequency, Hz, display domain
    int mode = -1;    // RADIO_IFACE_MODE_*, -1 = nothing stored; radioModeName() to show it
};

// The band stacking data layer: what is remembered per band, and what happens
// when the user asks for one.
//
// The F-INP band grid is a view over this. It draws keys and register lists and
// reports gestures; every decision -- which register, which frequency, which
// mode, what to write back, what a first visit means -- is made here.
//
// Threading contract:
//   - UI thread only. Nothing here is called from the tune path, so rigctl's
//     network thread (doc/bugs/ui-thread-sync.md) never reaches this state:
//     the current frequency is sampled, never pushed in.
//
// The config is the store. Each band owns three rotating optional entries;
// entry 0 is always current, so no separate register pointer exists. The
// instance keeps the band picker and lifecycle hooks behind one API. See
// doc/design/band-stack.md.
class BandStack {
public:
    // The band's three rotating slots. Unpopulated entries remain present so
    // pressing the active band can rotate into an empty slot and populate it.
    // Entry 0 is always the active register.
    std::vector<BandRegister> registersFor(const bandplan::Band_t& band) const;

    // Resolve within the visible band group. Opening/changing a group may
    // visibly select another service; it never writes a register.
    std::string activateBandForGroup(
        const std::string& group,
        double frequency);

    // A band-key tap. `activeBandId` is the visible source selected by
    // activateBandForGroup(), not a globally inferred write-back target.
    // Repeating that band stores entry 0, rotates left, and recalls the new 0.
    void selectBand(
        const bandplan::Band_t& band,
        const std::string& activeBandId,
        const std::string& group);

    // A register-list pick: store the visible source, rotate the picked target
    // entry to index 0 while preserving cyclic order, and recall it when set.
    void recallRegister(
        const bandplan::Band_t& band,
        int index,
        const std::string& activeBandId,
        const std::string& group);

    // Persist the current selector memory at a lifecycle boundary. Band
    // matching is restricted to the last group and current service; shutdown
    // never changes service.
    void commitCurrent();

    // Mode implied by a band's service and frequency, for bands whose plan entry
    // carries no def_mode. Keep in sync with heuristic_mode() in
    // scripts/enrich_bandplans.py.
    static int heuristicMode(const bandplan::Band_t& band);

private:
    void applyTarget(const bandplan::Band_t& band, double freq, int mode);
    void requestTune(double freq);  // the one seam onto gui::freqSelect
    int currentMode() const;        // -1 when the selected VFO is not a radio
};
