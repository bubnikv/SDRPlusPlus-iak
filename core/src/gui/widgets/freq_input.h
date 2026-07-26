#pragma once
#include <imgui.h>
#include <stdint.h>
#include <string>

namespace bandplan {
    struct Band_t;
}

// The F-INP direct-entry dialog: a modal with two pages, BAND and F-INP, opened
// by the top bar's keypad button or by a long press on a frequency digit.
//
// The digit widget in frequency_select.h owns one Dialog and hands it a Context
// each frame. Everything here is a view: the only tuning that happens is through
// gui::bandStack, which the band page is a view over, and the frequency the
// keypad page returns in its Outcome.
namespace freq_input {

    // The tuning situation a page works against: where the radio is now, and
    // what the source will accept. This is the (limitFreq, minFreq, maxFreq)
    // argument tail that every keypad helper used to thread through by hand.
    struct Context {
        uint64_t frequency = 0; // current display frequency, Hz
        bool limited = false;   // the source imposes a tuning range
        uint64_t minFreq = 0;
        uint64_t maxFreq = 0;

        // Range edges, normalised so lo <= hi. Both 0 when unlimited; callers
        // that care check `limited` first.
        uint64_t rangeLo() const { return limited ? ((minFreq < maxFreq) ? minFreq : maxFreq) : 0; }
        uint64_t rangeHi() const { return limited ? ((minFreq > maxFreq) ? minFreq : maxFreq) : 0; }
    };

    // Keypad grid geometry, computed once per frame by Dialog. Both pages are
    // sized from it so the popup keeps one width across pages, and so the dp()
    // constants exist in exactly one place -- the page toggle and the band grid
    // used to re-derive them from the keypad's own, free to drift silently.
    struct Metrics {
        ImVec2 keySize;           // one digit key
        float funcWidth = 0.0f;   // CE / Cancel / ENT column
        float totalWidth = 0.0f;  // full page width

        static Metrics compute();
    };

    // What a page asks of the dialog after a frame. Bands only ever closes: it
    // tunes through gui::bandStack itself. Keypad has no application reach at
    // all and returns its result here, which is what keeps keypad.cpp free of
    // every gui:: and config dependency.
    struct Outcome {
        bool close = false;
        bool commit = false;    // tune to `frequency`
        uint64_t frequency = 0;
    };

    // F-INP page: direct numeric entry in MHz, IC-705 style. See keypad.cpp.
    class Keypad {
    public:
        void onOpen();
        Outcome draw(const Context& ctx, const Metrics& m);

    private:
        void key(char k, const Context& ctx);
        void commit(const Context& ctx, Outcome& out) const;

        // The typed value in MHz: digits plus at most one decimal point.
        std::string entry;
    };

    // BAND page: a grid of band keys over the loaded band plan, backed by
    // gui::bandStack. See bands.cpp.
    class Bands {
    public:
        void onOpen();
        Outcome draw(const Context& ctx, const Metrics& m);

    private:
        // Selected category bucket ("Ham", ..., "All"), loaded from config on open.
        std::string category;

        // Band-key press tracking: a quick tap recalls the band's newest
        // register, a motionless hold opens its stacking-register list (IC-705:
        // "touch the band key for 1 second").
        int pressBand = -1;  // index into the shown[] grid, -1 = none
        bool longPressDone = false;
        const bandplan::Band_t* regPopupBand = nullptr; // band whose registers the popup lists
    };

    // The modal shell: the page toggle, the last-used page, and the keys that
    // apply to both pages. See dialog.cpp.
    class Dialog {
    public:
        // Request the dialog on the next draw(). The popup is opened inside
        // draw() so that OpenPopup lands at the same ImGui scope as the
        // matching BeginPopupModal.
        void open() { requestOpen = true; }

        Outcome draw(const Context& ctx);

    private:
        bool requestOpen = false;
        int page = 1; // 0 = BAND, 1 = F-INP
        Keypad keypad;
        Bands bands;
    };
}
