#pragma once
#include <imgui.h>
#include <stdint.h>
#include <string>

namespace bandplan {
    struct Band_t;
}

// The F-INP direct-entry dialog: a modal with BAND, SPECTRUM and F-INP pages, opened
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

    // The dialog's layout budget, recomputed each frame by Dialog. Every page
    // is sized from it, so the popup keeps one width across pages and the dp()
    // constants exist in exactly one place -- the page toggle and the band grid
    // used to re-derive them from the keypad's own, free to drift silently.
    //
    // Everything here is a function of the viewport and the style, never of the
    // content. A width that depended on what the grid holds would resize the
    // dialog as the user switches category; a height that depended on how much
    // the keypad has to say would move the keys out from under a finger
    // mid-entry. Both used to happen.
    struct Metrics {
        // Keypad block.
        ImVec2 keySize;             // one digit key
        float funcWidth = 0.0f;     // CE / Cancel / ENT column
        float keypadWidth = 0.0f;   // 3 digit columns + the function column
        bool wideKeypad = false;    // readout beside the keys, not above them

        // Page.
        float totalWidth = 0.0f;    // content width, identical on every page
        float pageHeight = 0.0f;    // height left for a page below the toggle row
        float toggleHeight = 0.0f;  // page toggle / close row
        float rowHeight = 0.0f;     // minimum interactive row (48 dp under touch)

        // Grids. The column count follows the width, so a viewport that is
        // short and wide -- a phone in landscape -- trades rows for columns and
        // fits instead of overflowing.
        int bandCols = 4;
        int spectrumCols = 3;
        float bandKeyHeight = 0.0f;
        float spectrumKeyHeight = 0.0f;

        // Width available to grid keys: `totalWidth` less a permanently
        // reserved scrollbar, so nothing shifts on the frame a grid starts
        // scrolling and the popup does not change width between pages.
        float gridWidth = 0.0f;

        // Whole rows of `keyH` that fit in `avail`, never fewer than two.
        static int rowsThatFit(float avail, float keyH);

        // Child height for a grid of `rowsNeeded` rows: exactly what it needs
        // when it fits, otherwise half a row short of the budget, the clipped
        // row being the hint that it scrolls.
        static float gridHeight(int rowsNeeded, int rowsFit, float keyH);

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
        // Stable ID rather than a pointer into the selected plan or the
        // per-frame canonical projection.
        std::string regPopupBandId;
        // Where that list opens: beside the key it belongs to, above or below
        // depending on which half of the viewport the key is in.
        ImVec2 regPopupPos = ImVec2(0.0f, 0.0f);
        ImVec2 regPopupPivot = ImVec2(0.0f, 0.0f);
    };

    // SPECTRUM page: a service-independent, non-overlapping ITU/IEEE range
    // selector. It tunes through Outcome and keeps its memory separate from the
    // service-band stacking registers. See spectrum.cpp.
    class Spectrum {
    public:
        void onOpen();
        Outcome draw(const Context& ctx, const Metrics& m);

    private:
        std::string lastRangeId;
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
        int page = 2; // 0 = BAND, 1 = SPECTRUM, 2 = F-INP
        // Re-centre once when the viewport changes size. Android handles
        // rotation live (configChanges in the manifest), so a dialog open
        // across one would otherwise be left off-centre or half off-screen.
        ImVec2 lastViewport = ImVec2(0.0f, 0.0f);
        bool recenter = false;
        Keypad keypad;
        Bands bands;
        Spectrum spectrum;
    };
}
