#pragma once
#include <gui/widgets/band_mapping.h>
#include <module.h>
#include <radio_interface.h>
#include <string>
#include <string_view>
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
// The config is the store. Each stable band_id owns three rotating optional
// entries; entry 0 is always current, so no separate register pointer exists.
// The instance keeps the band picker and lifecycle hooks behind one API. See
// doc/design/band-stack.md.
class BandStack {
public:
    // The band's three rotating slots. Unpopulated entries remain present so
    // pressing the active band can rotate into an empty slot and populate it.
    // Entry 0 is always the active register.
    std::vector<BandRegister> registersFor(std::string_view bandId) const;

    // Resolve within the services visible on the current picker page. The
    // supplied current service wins when it contains the frequency; otherwise
    // the first matching band from another visible service is used. Opening
    // or changing the page never writes a register.
    std::string activateBandForServices(
        freq_input::BandServiceSet services,
        freq_input::BandService currentService,
        double frequency);

    // A band-key tap. `activeBandId` is the visible source selected by
    // activateBandForServices(), not a globally inferred write-back target.
    // Repeating that band stores entry 0, rotates left, and recalls the new 0.
    void selectBand(
        std::string_view bandId,
        double defaultFrequency,
        const std::string& activeBandId);

    // Navigation fallback for a legacy row which has no stable identity. It
    // can tune the row but deliberately cannot own band-stack registers.
    void selectLegacySegment(
        const bandplan::Band_t& segment,
        const std::string& activeBandId);

    // A register-list pick: store the visible source, rotate the picked target
    // entry to index 0 while preserving cyclic order, and recall it when set.
    void recallRegister(
        std::string_view bandId,
        int index,
        const std::string& activeBandId);

    // Persist the current selector memory when closing or suspending the
    // application.
    // Band matching is restricted to the current service; shutdown never
    // changes service.
    void commitCurrent();

private:
    // Keep in sync with heuristic_mode() in scripts/enrich_bandplans.py.
    static int heuristicMode(
        freq_input::BandService service,
        freq_input::BandFamily family,
        double frequency);
    void applyTarget(
        const freq_input::BandMapping& mapping,
        double freq,
        int mode);
    void applySegmentTarget(
        const bandplan::Band_t& segment,
        double freq,
        int mode);
    void requestTune(double freq);  // the one seam onto gui::freqSelect
    int currentMode() const;        // -1 when the selected VFO is not a radio
};

namespace gui {
    SDRPP_EXPORT BandStack bandStack;
}
