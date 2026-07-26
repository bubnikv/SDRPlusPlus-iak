#pragma once
#include <radio_interface.h>
#include <vector>

namespace bandplan {
    struct Band_t;
}

// One band stacking register: a previously operated frequency and mode, in the
// IC-705 sense -- "when you change the operating band or the Register, the
// previously operated frequency and mode are stored". See
// doc/research/band-stacking.md for what the reference implementations keep.
struct BandRegister {
    double freq = 0;  // VFO tune frequency, Hz, display domain
    int mode = -1;    // RADIO_IFACE_MODE_*, -1 = nothing stored; radioModeName() to show it
};

// The band stacking data layer: what is remembered per band, and what happens
// when the user asks for one.
//
// The band grid in FrequencySelect is a view over this. It draws keys and
// register lists and reports gestures; every decision -- which register, which
// frequency, which mode, what to write back, what a first visit means -- is
// made here.
//
// Threading contract:
//   - UI thread only. Nothing here is called from the tune path, so rigctl's
//     network thread (doc/bugs/ui-thread-sync.md) never reaches this state:
//     the current frequency is sampled, never pushed in.
//
// Stateless for now -- the config is the store. The instance exists so that the
// state this needs next (which register each band is currently in, the
// uncommitted "last visited" entry, the commit timer) has a home that does not
// churn call sites. See doc/design/band-stack.md.
class BandStack {
public:
    // Registers stored for a band, newest first, dropping any entry no longer
    // inside the band's (possibly edited) edges. Empty for a band never visited.
    std::vector<BandRegister> registersFor(const bandplan::Band_t& band) const;

    // A tap on a band key: store the frequency and mode of the band being left,
    // then tune to this band's newest register -- or, on a first visit, to its
    // default (def_freq / def_mode, else the convention below).
    void selectBand(const bandplan::Band_t& band);

    // A pick from a band's register list: the same write-back, but tune to the
    // register the user chose. `index` indexes registersFor(band).
    void recallRegister(const bandplan::Band_t& band, int index);

    // Mode implied by a band's type and frequency, for bands whose plan entry
    // carries no def_mode. Keep in sync with heuristic_mode() in
    // scripts/enrich_bandplans.py.
    static int heuristicMode(const bandplan::Band_t& band);

private:
    void applyTarget(const bandplan::Band_t& band, double freq, int mode);
    void requestTune(double freq);  // the one seam onto gui::freqSelect
    int currentMode() const;        // -1 when the selected VFO is not a radio
};
