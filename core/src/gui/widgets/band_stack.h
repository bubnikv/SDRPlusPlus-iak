#pragma once
#include <gui/widgets/band_mapping.h>
#include <module.h>
#include <radio_interface.h>
#include <algorithm>
#include <array>
#include <cassert>
#include <optional>
#include <string>

namespace bandplan {
    struct Band_t;
    struct BandPlan_t;
}

// One band stacking register: a previously operated frequency and mode, in the
// IC-705 sense -- "when you change the operating band or the Register, the
// previously operated frequency and mode are stored". See
// doc/research/band-stacking.md for what the reference implementations keep.
struct BandRegister {
    double freq = 0;  // VFO tune frequency, Hz, display domain
    // RADIO_IFACE_MODE_*, or -1 when recall should resolve the current band-plan
    // default. Slot population is represented separately by std::optional.
    int mode = -1;
};

// One band's fixed three-register set, with slot 0 current. Empty positions are
// preserved because repeat selection rotates all three physical registers.
class BandRegisterSet {
public:
    static constexpr std::size_t SIZE = 3;
    using Slot = std::optional<BandRegister>;

    constexpr std::size_t size() const noexcept { return SIZE; }
    const Slot& operator[](std::size_t index) const {
        assert(index < SIZE);
        return slots[index];
    }
    const Slot& top() const { return slots[0]; }

    // Loader/building primitive which preserves the physical slot number.
    bool setSlot(std::size_t index, const BandRegister& reg) {
        if (index >= SIZE) { return false; }
        slots[index] = reg;
        return true;
    }
    bool seedTop(const BandRegister& reg) {
        if (slots[0]) { return false; }
        slots[0] = reg;
        return true;
    }
    void storeTop(const BandRegister& reg) { slots[0] = reg; }
    // Save the state being left, then rotate third -> top -> second -> third.
    // If the newly selected top was empty, initialize it from that saved state.
    void repeatWithCurrent(const BandRegister& current) {
        slots[0] = current;
        std::rotate(slots.begin(), slots.end() - 1, slots.end());
        if (!slots[0]) { slots[0] = current; }
    }
    // Select a populated slot, or first materialize the selected empty slot.
    // Returns the selected register now at slot 0, or nullopt on no target.
    std::optional<BandRegister> select(
        std::size_t index,
        std::optional<BandRegister> materialization = std::nullopt)
    {
        if (index >= SIZE) { return std::nullopt; }
        if (!slots[index]) {
            if (!materialization) { return std::nullopt; }
            slots[index] = *materialization;
        }
        std::rotate(
            slots.begin(),
            slots.begin() + index,
            slots.begin() + index + 1);
        return slots[0];
    }

private:
    std::array<Slot, SIZE> slots{};
};

enum class BandRecallResult {
    NoTarget,
    Recalled
};

// A register popup's stored baseline and current display. The baseline is
// captured when the popup opens and is never changed by display refreshes. The
// live overlay and empty-row capability are refreshed while the popup is open;
// recall validates the live state once more before committing anything.
struct BandRegisterPopupSnapshot {
    BandRegisterSet storedRegisters;
    BandRegisterSet registers;
    bool canMaterializeEmpty = false;
};

// Pure popup transitions used by BandStack and its contract tests. A live top
// is only a display overlay for the visibly active band. The stored baseline
// changes only when its top was empty and could be seeded during preparation.
struct BandRegisterPopupPreparation {
    BandRegisterPopupSnapshot snapshot;
    bool topSeeded = false;
};

BandRegisterPopupPreparation prepareBandRegisterPopup(
    const BandRegisterSet& storedRegisters,
    std::optional<BandRegister> current,
    std::optional<BandRegister> fallback,
    bool targetIsActive);

void refreshBandRegisterPopup(
    BandRegisterPopupSnapshot& snapshot,
    std::optional<BandRegister> current,
    bool targetIsActive);

// The band stacking data layer: what is remembered per band, and what happens
// when the user asks for one.
//
// The frequency-input band grid is a view over this. It draws keys and register lists and
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
    // A register-popup open. For the visibly active band, overlay the current
    // VFO on the returned snapshot without overwriting a populated stored top.
    // Seed an empty top from an in-band VFO, otherwise from the supplied band
    // default. Never tunes, rotates, or changes the active band.
    BandRegisterPopupSnapshot openRegisters(
        const freq_input::BandMapping& mapping,
        double defaultFrequency,
        const freq_input::BandMapping* activeMapping);

    // Refresh only the popup's live view. Never persists, tunes, or rotates.
    void refreshRegisterPopup(
        const freq_input::BandMapping& mapping,
        const freq_input::BandMapping* activeMapping,
        BandRegisterPopupSnapshot& snapshot);

    // Pure visible-page resolution. The preferred service wins when it contains
    // the frequency; otherwise the first match in another visible service wins.
    const freq_input::BandMapping* resolveBandForServices(
        freq_input::BandServiceSet services,
        freq_input::BandService preferredService,
        double frequency) const;

    // A band-key tap. `activeMapping` is the visible source selected by
    // resolveBandForServices(), not a globally inferred write-back target.
    // Repeating that band stores entry 0, rotates all three entries right,
    // initializes an empty new 0 from the saved current state, and recalls it.
    void selectBand(
        const freq_input::BandMapping& mapping,
        double defaultFrequency,
        const freq_input::BandMapping* activeMapping);

    // Navigation fallback for a legacy row which has no stable identity. It
    // can tune the row but deliberately cannot own band-stack registers.
    void selectLegacySegment(
        const bandplan::Band_t& segment,
        const freq_input::BandMapping* activeMapping);

    // A register-list pick: store the visible source, materialize a selected
    // empty slot only from an in-band VFO, rotate prefix [0, index] right once
    // to promote the target to slot 0, and recall it.
    BandRecallResult recallRegister(
        const freq_input::BandMapping& mapping,
        int index,
        const freq_input::BandMapping* activeMapping);

    // Persist the current band memory when closing or suspending the
    // application. Matching is restricted to the current service.
    void commitCurrentBand();

private:
    // Keep in sync with heuristic_mode() in scripts/enrich_bandplans.py.
    static int heuristicMode(
        freq_input::BandService service,
        freq_input::BandFamily family,
        double frequency);
    static int resolvedMode(
        const bandplan::Band_t& segment,
        double frequency,
        int mode);
    static std::optional<BandRegister> captureRegister(
        const bandplan::BandPlan_t* plan,
        const freq_input::BandMapping& mapping,
        double frequency,
        int mode);
    static std::optional<BandRegister> defaultRegister(
        const bandplan::BandPlan_t* plan,
        const freq_input::BandMapping& mapping,
        double frequency);
    void applyTarget(
        const freq_input::BandMapping& mapping,
        const BandRegister& target);
    void applySegmentTarget(
        const bandplan::Band_t& segment,
        double freq,
        int mode);
    int currentMode() const;        // -1 when the selected VFO is not a radio
};

namespace gui {
    SDRPP_EXPORT BandStack bandStack;
}
