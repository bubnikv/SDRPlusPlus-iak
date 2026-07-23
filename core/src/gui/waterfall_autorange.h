#pragma once
#include <imgui/imgui.h>

// Continuous ("sticky") waterfall auto-range, driven by the autoscale button.
//
// A short click does a one-shot fit of the display range to the current FFT.
// Holding the button latches a continuous fit that keeps re-tracking the live
// noise floor / signal population with asymmetric temporal smoothing, so the
// range follows a rising floor quickly but relaxes downward slowly instead of
// pumping. The latched state persists in the config.
//
// The range itself (fftMin/fftMax) is owned by MainWindow -- this class only
// borrows references to it so the manual sliders, config and auto-scaler all
// act on the same values.
class WaterfallAutoRange {
public:
    // Bind to MainWindow's fftMin/fftMax and seed the latch from config.
    void init(float* fftMin, float* fftMax, bool stickyFromConfig);

    // Draw the autoscale button (Material contrast glyph) at the current
    // cursor and handle the click-vs-hold gesture. `footprint` is the total
    // button size in pixels; the caller positions the cursor.
    void drawButton(float footprint, const ImVec4& tint);

    // Step the continuous fit (throttled to ~10 Hz internally). Call once per
    // frame after the controls child so it keeps running with the menu closed.
    void update();

    // True while continuous auto-range is latched; used to disable the manual
    // Min/Max sliders so the user isn't fighting the auto-scaler.
    bool sticky() const { return stickyEnabled; }

private:
    void oneShotFit();
    void setSticky(bool on);

    float* fftMin = nullptr;
    float* fftMax = nullptr;

    bool stickyEnabled = false;
    bool initialized = false; // animated range seeded from the first fit
    float btnHold = 0.0f;     // long-press timer
    bool longPressed = false; // long-press already latched this press
    float updateAccum = 0.0f; // throttles the continuous fit to ~10 Hz
};
