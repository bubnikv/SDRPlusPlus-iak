#pragma once
#include <imgui.h>
#include <imgui_internal.h>
#include <stdint.h>
#include <gui/widgets/freq_input.h>

class FrequencySelect {
public:
    FrequencySelect();
    void init();
    void draw();
    float getWidth();
    void setFrequency(int64_t freq);
    // Open the frequency-input dialog (same one the digit long-press opens).
    void openKeypad() { dialog.open(); }

    uint64_t frequency;
    bool frequencyChanged = false;
    bool digitHovered = false;

    bool limitFreq;
    uint64_t minFreq;
    uint64_t maxFreq;

private:
    void onPosChange();
    void incrementDigit(int i);
    void decrementDigit(int i);
    void moveCursorToDigit(int i);
    freq_input::Context inputContext() const;

    ImVec2 widgetPos;
    ImVec2 lastWidgetPos;

    int digits[12];
    ImVec2 digitBottomMins[12];
    ImVec2 digitTopMins[12];
    ImVec2 digitBottomMaxs[12];
    ImVec2 digitTopMaxs[12];

    // First digit visible and manipulated (based on maxFreq)
    int firstDigit = 0;
    // Cached maxFreq and limitFreq to detect a change of layout.
    uint64_t lastMaxFreq = 0;
    bool lastLimitFreq = false;
    uint64_t lastScaleEpoch = 0;

    float cachedWidth_ = 0.0f;
    float wheelAccum = 0.0f;

    // Press tracking on digits: a quick release steps the digit, a motionless
    // hold opens the frequency-input dialog.
    int pressDigit = -1;  // digit index the press started on, -1 = none
    int pressDir = 0;     // +1 = top half (increment), -1 = bottom half (decrement)
    bool longPressDone = false;

    // The frequency-input dialog this widget opens. It draws nothing until asked; see
    // freq_input.h for what it is handed and what it hands back.
    freq_input::Dialog dialog;
};
