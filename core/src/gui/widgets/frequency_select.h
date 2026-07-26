#pragma once
#include <imgui.h>
#include <imgui_internal.h>
#include <stdint.h>
#include <string>

namespace bandplan {
    struct Band_t;
}

class FrequencySelect {
public:
    FrequencySelect();
    void init();
    void draw();
    float getWidth();
    void setFrequency(int64_t freq);
    // Open the F-INP direct-entry keypad (same dialog the digit long-press opens).
    void openKeypad() { keypadRequestOpen = true; }

    uint64_t frequency;
    bool frequencyChanged = false;
    bool digitHovered = false;

    bool limitFreq;
    uint64_t minFreq;
    uint64_t maxFreq;

private:
    void onPosChange();
    void onResize();
    void incrementDigit(int i);
    void decrementDigit(int i);
    void moveCursorToDigit(int i);
    void drawKeypad();
    void drawKeypadPage(bool& close);
    void drawBandPage(bool& close, float totalWidth);
    void keypadKey(char key);
    void commitEntry();

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

    char buf[100];
    float cachedWidth_ = 0.0f;
    float wheelAccum = 0.0f;

    // Press tracking on digits: a quick release steps the digit, a motionless
    // hold opens the F-INP direct-entry keypad.
    int pressDigit = -1;  // digit index the press started on, -1 = none
    int pressDir = 0;     // +1 = top half (increment), -1 = bottom half (decrement)
    bool longPressDone = false;

    // F-INP keypad state. `entry` holds the typed value in MHz: digits plus at
    // most one decimal point, IC-705 style.
    bool keypadRequestOpen = false;
    std::string entry;

    // Band picker page state, loaded from config each time the dialog opens.
    int page = 1;         // 0 = band grid, 1 = keypad
    std::string category; // selected category bucket ("Ham", ..., "All")

    // Band-key press tracking: a quick tap recalls the band's newest register,
    // a motionless hold opens its stacking-register list (IC-705: "touch the
    // band key for 1 second").
    int pressBand = -1;   // index into the shown[] grid, -1 = none
    bool bandLongPressDone = false;
    const bandplan::Band_t* regPopupBand = nullptr; // band whose registers the popup lists
};
