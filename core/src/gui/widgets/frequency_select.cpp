#include <gui/widgets/frequency_select.h>
#include <gui/widgets/bandplan.h>
#include <gui/widgets/popup_dialog.h>
#include <config.h>
#include <core.h>
#include <radio_interface.h>
#include <gui/style.h>
#include <gui/gui.h>
#include <backend.h>
#include <utils/hrfreq.h>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <cmath>
#include <algorithm>
#include <vector>
#ifdef __ANDROID__
#include <android_backend.h>
#endif

#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include <imgui/imgui_internal.h>

bool isInArea(ImVec2 val, ImVec2 min, ImVec2 max) {
    return val.x >= min.x && val.x < max.x && val.y >= min.y && val.y < max.y;
}

FrequencySelect::FrequencySelect() {
}

void FrequencySelect::init() {
    for (int i = 0; i < 12; i++) {
        digits[i] = 0;
    }
}

void FrequencySelect::onPosChange() {
    // Round the glyph metrics (fractional at fractional UI scales) so this
    // computation stays identical to getWidth() — both must produce the same
    // cachedWidth_ or the top-bar layout shifts whenever the other one runs.
    ImVec2 digitSz = ImGui::CalcTextSize("0");
    ImVec2 commaSz = ImGui::CalcTextSize(".");
    int digitHeight = (int)roundf(digitSz.y);
    int digitWidth = (int)roundf(digitSz.x);
    int commaWidth = (int)roundf(commaSz.x);
    int commaOffset = 0;
    for (int i = firstDigit; i < 12; i++) {
        int pos = i - firstDigit;
        digitTopMins[i] = ImVec2(widgetPos.x + (pos * digitWidth) + commaOffset, widgetPos.y);
        digitBottomMins[i] = ImVec2(widgetPos.x + (pos * digitWidth) + commaOffset, widgetPos.y + (digitHeight / 2));

        digitTopMaxs[i] = ImVec2(widgetPos.x + (pos * digitWidth) + commaOffset + digitWidth, widgetPos.y + (digitHeight / 2));
        digitBottomMaxs[i] = ImVec2(widgetPos.x + (pos * digitWidth) + commaOffset + digitWidth, widgetPos.y + digitHeight);

        if ((i + 1) % 3 == 0 && i < 11) {
            commaOffset += commaWidth;
        }
    }
    // commaOffset now holds the total accumulated comma width — reuse it for the
    // width cache rather than recomputing with a separate PushFont/CalcTextSize pair.
    cachedWidth_ = (12 - firstDigit) * digitWidth + commaOffset + style::dp(17.0f);
}

void FrequencySelect::incrementDigit(int i) {
    if (i < 0) {
        return;
    }
    if (digits[i] < 9) {
        digits[i]++;
    }
    else {
        digits[i] = 0;
        incrementDigit(i - 1);
    }
    frequencyChanged = true;
}

void FrequencySelect::decrementDigit(int i) {
    if (i < 0) {
        return;
    }
    if (digits[i] > 0) {
        digits[i]--;
    }
    else {
        if (i == 0) { return; }

        // Check if there are non zero digits afterwards
        bool otherNoneZero = false;
        for (int j = i - 1; j >= 0; j--) {
            if (digits[j] > 0) {
                otherNoneZero = true;
                break;
            }
        }
        if (!otherNoneZero) { return; }

        digits[i] = 9;
        decrementDigit(i - 1);
    }
    frequencyChanged = true;
}

void FrequencySelect::moveCursorToDigit(int i) {
    double xpos, ypos;
    backend::getMouseScreenPos(xpos, ypos);
    double nxpos = (digitTopMaxs[i].x + digitTopMins[i].x) / 2.0;
    backend::setMouseScreenPos(nxpos, ypos);
}

float FrequencySelect::getWidth() {
    // getWidth() is called during top-bar layout before draw() runs, so it
    // must refresh the cache itself when the scale changes — otherwise the
    // first scaled frame reserves space using old-scale digit widths.
    uint64_t currentScaleEpoch = style::scaleEpoch();
    if (currentScaleEpoch != lastScaleEpoch || cachedWidth_ == 0.0f) {
        ImGui::PushFont(style::bigFont);
        // Same rounded metrics as onPosChange() — the two must agree exactly.
        int digitWidth = (int)roundf(ImGui::CalcTextSize("0").x);
        int commaWidth = (int)roundf(ImGui::CalcTextSize(".").x);
        ImGui::PopFont();
        int commaCount = 0;
        for (int i = firstDigit; i < 11; i++) {
            if ((i + 1) % 3 == 0) { commaCount++; }
        }
        cachedWidth_ = (12 - firstDigit) * digitWidth + commaCount * commaWidth + style::dp(17.0f);
        // Don't update lastScaleEpoch here — draw() still needs to see the
        // change to recompute the digit position arrays.
    }
    return cachedWidth_;
}

void FrequencySelect::draw() {
    auto window = ImGui::GetCurrentWindow();
    auto io = ImGui::GetIO();
    widgetPos = ImGui::GetWindowContentRegionMin();
    ImVec2 cursorPos = ImGui::GetCursorPos();
    widgetPos.x += window->Pos.x + cursorPos.x;
    ImGui::PushFont(style::bigFont);
    ImVec2 digitSz = ImGui::CalcTextSize("0");
    ImVec2 commaSz = ImGui::CalcTextSize(".");
    // Snap the origin to whole pixels — a fractional baseline blurs the big
    // digits at fractional UI scales.
    widgetPos.x = roundf(widgetPos.x);
    widgetPos.y = roundf(window->Pos.y + cursorPos.y - ((digitSz.y / 2.0f) - ceilf(15 * style::uiScale) - 5));

    // Recompute the first visible digit only when maxFreq or limitFreq changes
    bool firstDigitChanged = false;
    if (maxFreq != lastMaxFreq || limitFreq != lastLimitFreq) {
        lastMaxFreq = maxFreq;
        lastLimitFreq = limitFreq;
        int newFirstDigit = 0;
        if (limitFreq && maxFreq > 0) {
            uint64_t mf = maxFreq;
            int numDigits = 0;
            while (mf > 0) { mf /= 10; numDigits++; }
            newFirstDigit = 12 - numDigits;
            if (newFirstDigit < 0) { newFirstDigit = 0; }
        }
        firstDigitChanged = (newFirstDigit != firstDigit);
        firstDigit = newFirstDigit;
        // Zero out hidden leading digits
        for (int i = 0; i < firstDigit; i++)
            digits[i] = 0;
    }

    uint64_t currentScaleEpoch = style::scaleEpoch();
    if (widgetPos.x != lastWidgetPos.x || widgetPos.y != lastWidgetPos.y || firstDigitChanged || currentScaleEpoch != lastScaleEpoch) {
        lastWidgetPos = widgetPos;
        lastScaleEpoch = currentScaleEpoch;
        onPosChange();
    }

    ImU32 disabledColor = ImGui::GetColorU32(ImGuiCol_Text, 0.3f);
    ImU32 textColor = ImGui::GetColorU32(ImGuiCol_Text);

    
    // Same rounded metrics as onPosChange() so drawn digits line up with the
    // cached hitboxes.
    int digitWidth = (int)roundf(digitSz.x);
    int commaWidth = (int)roundf(commaSz.x);
    int commaOffset = 0;
    float textOffset = (float)style::scale(11.0f);
    bool zeros = true;

    ImGui::ItemSize(ImRect(digitTopMins[firstDigit], ImVec2(digitBottomMaxs[11].x + style::dp(17.0f), digitBottomMaxs[11].y)));

    for (int i = firstDigit; i < 12; i++) {
        if (digits[i] != 0) {
            zeros = false;
        }
        int pos = i - firstDigit;
        sprintf(buf, "%d", digits[i]);
        window->DrawList->AddText(ImVec2(widgetPos.x + (pos * digitWidth) + commaOffset, widgetPos.y),
                                  zeros ? disabledColor : textColor, buf);
        if ((i + 1) % 3 == 0 && i < 11) {
            commaOffset += commaWidth;
            window->DrawList->AddText(ImVec2(widgetPos.x + (pos * digitWidth) + commaOffset + textOffset, widgetPos.y),
                                      zeros ? disabledColor : textColor, ".");
        }
    }

    bool hovered = false;
    if (ImGui::IsWindowHovered(ImGuiHoveredFlags_None) &&
        !gui::mainWindow.lockWaterfallControls)
    {
        ImVec2 mousePos = ImGui::GetMousePos();
        bool leftClick = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
        bool rightClick = ImGui::IsMouseClicked(ImGuiMouseButton_Right);
        // Precision touchpads report fractional wheel deltas; accumulate and
        // step digits on whole notches instead of truncating them away.
        wheelAccum += io.MouseWheel;
        int mw = (int)wheelAccum;
        wheelAccum -= (float)mw;
        bool onDigit = false;

        for (int i = firstDigit; i < 12; i++) {
            onDigit = false;
            if (isInArea(mousePos, digitTopMins[i], digitTopMaxs[i])) {
                window->DrawList->AddRectFilled(digitTopMins[i], digitTopMaxs[i], IM_COL32(255, 0, 0, 75));
                if (leftClick) {
                    pressDigit = i;
                    pressDir = 1;
                    longPressDone = false;
                }
                onDigit = true;
            }
            if (isInArea(mousePos, digitBottomMins[i], digitBottomMaxs[i])) {
                window->DrawList->AddRectFilled(digitBottomMins[i], digitBottomMaxs[i], IM_COL32(0, 0, 255, 75));
                if (leftClick) {
                    pressDigit = i;
                    pressDir = -1;
                    longPressDone = false;
                }
                onDigit = true;
            }
            if (onDigit) {
                hovered = true;
                if (rightClick || ImGui::IsKeyPressed(ImGuiKey_Delete) || PopupDialog::confirmKeyPressed()) {
                    for (int j = i; j < 12; j++) {
                        digits[j] = 0;
                    }

                    frequencyChanged = true;
                }
                if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
                    incrementDigit(i);
                }
                if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
                    decrementDigit(i);
                }
                if ((ImGui::IsKeyPressed(ImGuiKey_LeftArrow) || ImGui::IsKeyPressed(ImGuiKey_Backspace)) && i > firstDigit) {
                    moveCursorToDigit(i - 1);
                }
                if (ImGui::IsKeyPressed(ImGuiKey_RightArrow) && i < 11) {
                    moveCursorToDigit(i + 1);
                }

                auto chars = io.InputQueueCharacters;

                // For each keyboard characters, type it
                for (int j = 0; j < chars.Size; j++) {
                    if (chars[j] >= '0' && chars[j] <= '9') {
                        if ((i + j) > 11) { break; }
                        digits[i + j] = chars[j] - '0';
                        if ((i + j) < 11) { moveCursorToDigit(i + j + 1); }
                        frequencyChanged = true;
                    }
                }

                if (mw != 0) {
                    int count = abs(mw);
                    for (int j = 0; j < count; j++) {
                        mw > 0 ? incrementDigit(i) : decrementDigit(i);
                    }
                }
            }
        }

        // A press armed on a digit half steps it on a quick release; held
        // motionless past the threshold it opens the F-INP keypad instead.
        // Stepping waits for the release so the two can't both fire.
        if (pressDigit >= 0) {
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                float slop = 10.0f * style::uiScale;
                float dx = mousePos.x - io.MouseClickedPos[ImGuiMouseButton_Left].x;
                float dy = mousePos.y - io.MouseClickedPos[ImGuiMouseButton_Left].y;
                if ((dx * dx) + (dy * dy) > (slop * slop)) {
                    pressDigit = -1; // moved away: neither a tap nor a long press
                }
                else if (!longPressDone && io.MouseDownDuration[ImGuiMouseButton_Left] >= 0.5f) {
                    longPressDone = true;
                    keypadRequestOpen = true;
#ifdef __ANDROID__
                    backend::hapticTick();
#endif
                }
            }
            else if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                if (!longPressDone) {
                    if (pressDir > 0 && isInArea(mousePos, digitTopMins[pressDigit], digitTopMaxs[pressDigit])) {
                        incrementDigit(pressDigit);
                    }
                    else if (pressDir < 0 && isInArea(mousePos, digitBottomMins[pressDigit], digitBottomMaxs[pressDigit])) {
                        decrementDigit(pressDigit);
                    }
                }
                pressDigit = -1;
            }
            else {
                pressDigit = -1; // press was lost without a release event
            }
        }

        if (isInArea(mousePos, digitTopMins[firstDigit], digitBottomMaxs[11])) {
            bool shortcutKey = io.ConfigMacOSXBehaviors ? (io.KeyMods == ImGuiKeyModFlags_Super) : (io.KeyMods == ImGuiKeyModFlags_Ctrl);
            bool ctrlOnly = (io.KeyMods == ImGuiKeyModFlags_Ctrl);
            bool shiftOnly = (io.KeyMods == ImGuiKeyModFlags_Shift);
            bool copy  = ((shortcutKey && ImGui::IsKeyPressed(ImGuiKey_C)) || (ctrlOnly  && ImGui::IsKeyPressed(ImGuiKey_Insert)));
            bool paste = ((shortcutKey && ImGui::IsKeyPressed(ImGuiKey_V)) || (shiftOnly && ImGui::IsKeyPressed(ImGuiKey_Insert)));
            if (copy) {
                // Convert the freqency to a string
                std::string freqStr = hrfreq::toString(frequency);

                // Write it to the clipboard
                ImGui::SetClipboardText(freqStr.c_str());
            }
            if (paste) {
                // Attempt to parse the clipboard as a number
                const char* clip = ImGui::GetClipboardText();

                // If the clipboard is not empty, attempt to parse it
                if (clip) {
                    double newFreq;
                    if (hrfreq::fromString(clip, newFreq)) {
                        setFrequency(abs(newFreq));
                        frequencyChanged = true;
                    }
                }
            }
        }
    }
    // Assigned outside the hover-gated block: leaving the window while over a
    // digit used to leave digitHovered stuck true, blocking the arrow-key
    // tuning in main_window. Partial wheel notches are also dropped once the
    // cursor is off the digits so they can't discharge into a step later.
    digitHovered = hovered;
    if (!hovered) { wheelAccum = 0.0f; }

    uint64_t freq = 0;
    for (int i = 0; i < 12; i++) {
        freq += digits[i] * pow(10, 11 - i);
    }

    uint64_t orig = freq;
    freq = std::clamp<uint64_t>(freq, minFreq, maxFreq);
    if (freq != orig && limitFreq) {
        setFrequency(freq);
    }
    else {
        frequency = orig;
    }

    ImGui::PopFont();

    ImGui::SetCursorPosX(digitBottomMaxs[11].x + (float)style::scale(17.0f));

    drawKeypad();
}

// Format a frequency in Hz with '.' group separators, matching the widget.
static std::string groupHz(uint64_t hz) {
    std::string s = std::to_string(hz);
    for (int pos = (int)s.size() - 3; pos > 0; pos -= 3) {
        s.insert(pos, ".");
    }
    return s;
}

static int digitCount(uint64_t value) {
    int count = 1;
    while (value >= 10) {
        value /= 10;
        count++;
    }
    return count;
}

static int keypadIntegerDigitLimit(bool limitFreq, uint64_t maxFreq) {
    if (!limitFreq) { return 6; } // dialog entry is MHz, capped at 999999 MHz
    uint64_t maxMHz = maxFreq / 1000000;
    return std::min(6, digitCount(maxMHz));
}

static uint64_t clampKeypadHz(double hz, bool limitFreq, uint64_t minFreq, uint64_t maxFreq) {
    if (!std::isfinite(hz) || hz < 0.0) { hz = 0.0; }
    hz = std::min(hz, 999999999999.0);

    if (limitFreq) {
        const uint64_t lo = std::min(minFreq, maxFreq);
        const uint64_t hi = std::max(minFreq, maxFreq);
        hz = std::clamp(hz, (double)lo, (double)hi);
    }

    return (uint64_t)hz;
}

void FrequencySelect::keypadKey(char key) {
    if (key >= '0' && key <= '9') {
        size_t dot = entry.find('.');
        if (dot == std::string::npos) {
            if (entry == "0") { entry.clear(); }
            uint64_t activeMax = limitFreq ? std::max(minFreq, maxFreq) : maxFreq;
            if ((int)entry.size() < keypadIntegerDigitLimit(limitFreq, activeMax)) { entry += key; }
        }
        else if (entry.size() - dot - 1 < 6) { // down to 1 Hz
            entry += key;
        }
    }
    else if (key == '.') {
        if (entry.empty()) {
            // IC-705 shorthand: [.] first re-enters the current MHz digits, so
            // retuning within the band is just [.] plus the kHz digits.
            uint64_t currentHz = clampKeypadHz((double)frequency, limitFreq, minFreq, maxFreq);
            entry = std::to_string(currentHz / 1000000) + '.';
        }
        else if (entry.find('.') == std::string::npos) {
            entry += '.';
        }
    }
}

void FrequencySelect::commitEntry() {
    if (entry.empty()) { return; }
    // The entry is a decimal number in MHz; digits left blank below the last
    // entered one become zeros, same as the IC-705's [ENT].
    uint64_t hz = clampKeypadHz(round(atof(entry.c_str()) * 1e6), limitFreq, minFreq, maxFreq);
    setFrequency((int64_t)hz);
    frequencyChanged = true;
}

// Coarse filter bucket for the band picker's category row.
static const char* bandCategory(const std::string& type) {
    if (type == "amateur" || type == "amateur1") { return "Ham"; }
    if (type == "broadcast") { return "Bcast"; }
    if (type == "aviation" || type == "aircraft") { return "Air"; }
    if (type == "marine" || type == "marine1") { return "Marine"; }
    return "Util";
}

// Index order matches the RADIO_IFACE_MODE_* enum.
static const char* const kRadioModeNames[] = { "NFM", "WFM", "AM", "DSB", "USB", "CW", "LSB", "RAW", "CWR" };

static int radioModeFromString(const std::string& mode) {
    for (int i = 0; i < 9; i++) {
        if (mode == kRadioModeNames[i]) { return i; }
    }
    return -1;
}

static const char* radioModeName(int mode) {
    return (mode >= RADIO_IFACE_MODE_NFM && mode <= RADIO_IFACE_MODE_CWR) ? kRadioModeNames[mode] : "--";
}

// Frequency in MHz, trailing zeros trimmed: 14025000 -> "14.025".
static std::string mhzExact(double hz) {
    char b[32];
    snprintf(b, sizeof(b), "%.6f", hz / 1e6);
    char* e = b + strlen(b) - 1;
    while (*e == '0') { *e-- = 0; }
    if (*e == '.') { *e = 0; }
    return b;
}

// A stacking register: previously operated frequency (Hz) and mode (enum, -1 if
// none). Newest first.
struct BandRegister {
    double freq;
    int mode;
};

// A band's stored registers, newest first, tolerating the legacy single-object
// format and dropping any entry no longer inside the (possibly changed) band
// edges — the same containment revalidation the single-slot design used.
static std::vector<BandRegister> readBandRegisters(const json& mem, const bandplan::Band_t& band) {
    std::vector<BandRegister> regs;
    auto it = mem.find(band.name);
    if (it == mem.end()) { return regs; }
    auto pushOne = [&](const json& o) {
        if (!o.is_object()) { return; }
        double f = o.value("freq", 0.0);
        if (f < band.start || f > band.end) { return; }
        int m = o.value("mode", -1);
        if (m < RADIO_IFACE_MODE_NFM || m > RADIO_IFACE_MODE_CWR) { m = -1; }
        regs.push_back({ f, m });
    };
    if (it->is_array()) {
        for (const auto& e : *it) {
            pushOne(e);
            if (regs.size() >= 3) { break; }
        }
    }
    else {
        pushOne(*it); // legacy single register
    }
    return regs;
}

// Push freq/mode to the front of a band's register stack: newest first, deduped
// by frequency, capped at 3. IC-705: "when you change the operating band or the
// Register, the previously operated frequency and mode are stored."
static void pushBandRegister(json& mem, const std::string& name, double freq, int mode) {
    json existing = json::array();
    auto it = mem.find(name);
    if (it != mem.end()) {
        if (it->is_array()) { existing = *it; }
        else if (it->is_object()) { existing.push_back(*it); } // migrate legacy
    }
    json out = json::array();
    out.push_back({ { "freq", freq }, { "mode", mode } });
    for (const auto& e : existing) {
        if (out.size() >= 3) { break; }
        if (!e.is_object()) { continue; }
        if (std::abs(e.value("freq", 0.0) - freq) < 1.0) { continue; } // dedup same frequency
        out.push_back(e);
    }
    mem[name] = out;
}

// Band-type/frequency mode convention applied when a band carries no def_mode.
// Keep in sync with heuristic_mode() in scripts/enrich_bandplans.py.
static int heuristicRadioMode(const bandplan::Band_t& b) {
    if (b.type == "amateur" || b.type == "amateur1") {
        if (b.end <= 600000.0) { return RADIO_IFACE_MODE_CW; }                            // 2200 m / 630 m
        if (b.start >= 5200000.0 && b.start <= 5500000.0) { return RADIO_IFACE_MODE_USB; } // 60 m channels
        if (b.start < 10000000.0) { return RADIO_IFACE_MODE_LSB; }
        if (b.start < 100000000.0) { return RADIO_IFACE_MODE_USB; }                       // 30 m .. 6 m/4 m
        return RADIO_IFACE_MODE_NFM;                                                      // 2 m and up: repeaters
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

// "40m Ham Band" -> "40m"; empty when the name has no wavelength token.
static std::string wavelengthToken(const std::string& name) {
    size_t n = name.size();
    for (size_t i = 0; i < n; i++) {
        if (!isdigit((unsigned char)name[i])) { continue; }
        if (i > 0 && isalnum((unsigned char)name[i - 1])) { continue; }
        size_t j = i;
        while (j < n && (isdigit((unsigned char)name[j]) || name[j] == '.')) { j++; }
        size_t k = j;
        while (k < n && isalpha((unsigned char)name[k])) { k++; }
        std::string unit = name.substr(j, k - j);
        if (unit == "m" || unit == "cm" || unit == "mm") {
            return name.substr(i, k - i);
        }
        i = k;
    }
    return "";
}

// Compact MHz main label for a band key: "1.8", "10.1", "144".
static std::string mhzLabel(double hz) {
    double mhz = hz / 1e6;
    char buf[16];
    if (mhz >= 20.0) { snprintf(buf, sizeof(buf), "%.0f", mhz); }
    else if (mhz >= 1.0) { snprintf(buf, sizeof(buf), "%.1f", mhz); }
    else { snprintf(buf, sizeof(buf), "%.2f", mhz); }
    if (strchr(buf, '.')) {
        char* end = buf + strlen(buf) - 1;
        while (*end == '0') { *end-- = 0; }
        if (*end == '.') { *end = 0; }
    }
    return buf;
}

// Centered AddText that shrinks the font size to fit maxWidth. bigFont only
// covers '.'-'9', so callers pass baseFont for any label containing letters.
static void centeredLabel(ImDrawList* dl, ImFont* font, float size, ImVec2 center, float maxWidth, ImU32 col, const char* text) {
    ImVec2 ts = font->CalcTextSizeA(size, FLT_MAX, 0.0f, text);
    if (ts.x > maxWidth && ts.x > 0.0f) {
        size *= maxWidth / ts.x;
        ts = font->CalcTextSizeA(size, FLT_MAX, 0.0f, text);
    }
    dl->AddText(font, size, ImVec2(center.x - ts.x / 2.0f, center.y - ts.y / 2.0f), col, text);
}

// Segmented-control button: the selected segment is drawn pressed.
static bool segButton(const char* label, bool selected, ImVec2 size) {
    if (selected) { ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive)); }
    bool clicked = ImGui::Button(label, size);
    if (selected) { ImGui::PopStyleColor(); }
    return clicked;
}

void FrequencySelect::drawKeypad() {
    if (keypadRequestOpen) {
        keypadRequestOpen = false;
        entry.clear();
        pressBand = -1;
        bandLongPressDone = false;
        regPopupBand = nullptr;
        // Last-used page/category, and the selected band plan (independent of
        // the bandPlanEnabled display toggle).
        core::configManager.acquire();
        auto& conf = core::configManager.conf;
        page = (conf.value("freqEntryPage", "keypad") == "band") ? 0 : 1;
        category = conf.value("freqEntryCategory", "Ham");
        planName = conf.value("bandPlan", "General");
        core::configManager.release();
        ImGui::OpenPopup("F-INP##sdrpp_freq_keypad");
    }

    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (!ImGui::BeginPopupModal("F-INP##sdrpp_freq_keypad", NULL, ImGuiWindowFlags_AlwaysAutoResize)) { return; }

    // Page toggle, sized to the keypad grid width so the popup width matches
    // on both pages.
    ImVec2 tsp = ImGui::GetStyle().ItemSpacing;
    float totalWidth = 3.0f * style::dp(56.0f) + 3.0f * tsp.x + style::dp(84.0f);
    float halfWidth = (totalWidth - tsp.x) / 2.0f;
    ImVec2 toggleSz(halfWidth, style::dp(34.0f));
    int newPage = page;
    if (segButton("BAND##sdrpp_finp_page", page == 0, toggleSz)) { newPage = 0; }
    ImGui::SameLine();
    if (segButton("F-INP##sdrpp_finp_page", page == 1, toggleSz)) { newPage = 1; }
    if (newPage != page) {
        page = newPage;
        core::configManager.acquire();
        core::configManager.conf["freqEntryPage"] = (page == 0) ? "band" : "keypad";
        core::configManager.release(true);
    }
    ImGui::Spacing();

    bool close = false;
    if (page == 0) { drawBandPage(close, totalWidth); }
    else { drawKeypadPage(close); }

    if (PopupDialog::cancelKeyPressed()) { close = true; }
    if (close) { ImGui::CloseCurrentPopup(); }
    ImGui::EndPopup();
}

void FrequencySelect::drawKeypadPage(bool& close) {
    ImGuiIO& kio = ImGui::GetIO();
    ImVec2 keySz(style::dp(56.0f), style::dp(42.0f));
    const bool hasRange = limitFreq;
    const uint64_t rangeLo = hasRange ? std::min(minFreq, maxFreq) : 0;
    const uint64_t rangeHi = hasRange ? std::max(minFreq, maxFreq) : 0;
    const ImVec4 errorCol(1.0f, 0.20f, 0.12f, 1.0f);
    double rawHz = 0.0;
    uint64_t targetHz = 0;
    bool belowRange = false;
    bool aboveRange = false;
    if (!entry.empty()) {
        rawHz = std::min(round(atof(entry.c_str()) * 1e6), 999999999999.0);
        targetHz = clampKeypadHz(rawHz, limitFreq, minFreq, maxFreq);
        belowRange = hasRange && rawHz < (double)rangeLo;
        aboveRange = hasRange && rawHz > (double)rangeHi;
    }
    const bool outOfRange = belowRange || aboveRange;

    // Entered value in MHz; before any key, the current frequency dimmed.
    ImGui::PushFont(style::bigFont);
    if (entry.empty()) {
        char cur[32];
        snprintf(cur, sizeof(cur), "%.6f", (double)frequency / 1e6);
        char* end = cur + strlen(cur) - 1;
        while (*end == '0') { *end-- = 0; }
        if (*end == '.') { *end = 0; }
        ImGui::TextDisabled("%s", cur);
    }
    else {
        if (outOfRange) { ImGui::PushStyleColor(ImGuiCol_Text, errorCol); }
        ImGui::TextUnformatted(entry.c_str());
        if (outOfRange) { ImGui::PopStyleColor(); }
    }
    ImGui::PopFont();

    if (entry.empty()) {
        ImGui::TextDisabled("Enter frequency in MHz");
    }
    else {
        if (outOfRange) { ImGui::PushStyleColor(ImGuiCol_Text, errorCol); }
        if (limitFreq && targetHz != (uint64_t)rawHz) {
            ImGui::Text("= %s Hz -> %s Hz", groupHz((uint64_t)rawHz).c_str(), groupHz(targetHz).c_str());
        }
        else {
            ImGui::Text("= %s Hz", groupHz(targetHz).c_str());
        }
        if (outOfRange) { ImGui::PopStyleColor(); }
        if (belowRange) {
            ImGui::TextColored(errorCol, "Below source range: minimum is %s Hz", groupHz(rangeLo).c_str());
        }
        else if (aboveRange) {
            ImGui::TextColored(errorCol, "Above source range: maximum is %s Hz", groupHz(rangeHi).c_str());
        }
    }
    if (hasRange) {
        ImGui::TextDisabled("Range: %s - %s Hz", groupHz(rangeLo).c_str(), groupHz(rangeHi).c_str());
    }
    ImGui::Spacing();

    // 4x4 grid, placed explicitly since the double-height ENT would otherwise
    // stretch its row: digit block with the backspace at its bottom-right
    // (Android PIN-pad convention), function column CE / Cancel / tall ENT.
    ImVec2 sp = ImGui::GetStyle().ItemSpacing;
    float fnW = style::dp(84.0f);
    ImVec2 origin = ImGui::GetCursorPos();
    auto cellPos = [&](int r, int c) {
        return ImVec2(origin.x + c * (keySz.x + sp.x), origin.y + r * (keySz.y + sp.y));
    };

    const char* dig[4][3] = {
        { "7", "8", "9" },
        { "4", "5", "6" },
        { "1", "2", "3" },
        { ".", "0", NULL } // NULL = backspace, drawn below
    };
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 3; c++) {
            if (!dig[r][c]) { continue; }
            ImGui::SetCursorPos(cellPos(r, c));
            if (ImGui::Button(dig[r][c], keySz)) { keypadKey(dig[r][c][0]); }
        }
    }

    // Backspace key: undoes the last keypress. Roboto-Medium ships no arrow or
    // erase glyph, so the icon is drawn by hand; GetColorU32 picks up the
    // disabled-state alpha automatically.
    ImGui::SetCursorPos(cellPos(3, 2));
    ImGui::BeginDisabled(entry.empty());
    bool bksp = ImGui::Button("##sdrpp_finp_bksp", keySz);
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 bmin = ImGui::GetItemRectMin();
        ImVec2 bmax = ImGui::GetItemRectMax();
        ImVec2 ctr((bmin.x + bmax.x) / 2.0f, (bmin.y + bmax.y) / 2.0f);
        float iw = style::dp(11.0f);    // icon half-width
        float ih = style::dp(7.0f);     // icon half-height
        float notch = style::dp(7.0f);  // depth of the pointed tip
        float th = style::dp(1.5f);
        ImU32 col = ImGui::GetColorU32(ImGuiCol_Text);
        ImVec2 pts[5] = {
            ImVec2(ctr.x - iw, ctr.y),
            ImVec2(ctr.x - iw + notch, ctr.y - ih),
            ImVec2(ctr.x + iw, ctr.y - ih),
            ImVec2(ctr.x + iw, ctr.y + ih),
            ImVec2(ctr.x - iw + notch, ctr.y + ih)
        };
        dl->AddPolyline(pts, 5, col, ImDrawFlags_Closed, th);
        float bx = ctr.x + notch / 2.0f; // center of the icon body
        float xr = style::dp(3.0f);
        dl->AddLine(ImVec2(bx - xr, ctr.y - xr), ImVec2(bx + xr, ctr.y + xr), col, th);
        dl->AddLine(ImVec2(bx - xr, ctr.y + xr), ImVec2(bx + xr, ctr.y - xr), col, th);
    }
    ImGui::EndDisabled();
    if (bksp) { entry.pop_back(); }

    ImGui::SetCursorPos(cellPos(0, 3));
    if (ImGui::Button("CE", ImVec2(fnW, keySz.y))) { entry.clear(); }
    ImGui::SetCursorPos(cellPos(1, 3));
    if (ImGui::Button("Cancel##sdrpp_finp", ImVec2(fnW, keySz.y))) { close = true; }
    ImGui::SetCursorPos(cellPos(2, 3));
    if (ImGui::Button("ENT##sdrpp_finp", ImVec2(fnW, 2.0f * keySz.y + sp.y))) {
        commitEntry();
        close = true;
    }

    // Hardware keyboard: digits, '.', Backspace, Enter (Escape is handled
    // page-independently in drawKeypad).
    for (int j = 0; j < kio.InputQueueCharacters.Size; j++) {
        char c = (char)kio.InputQueueCharacters[j];
        if ((c >= '0' && c <= '9') || c == '.') { keypadKey(c); }
        else if (c == ',') { keypadKey('.'); }
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Backspace) && !entry.empty()) { entry.pop_back(); }
    if (PopupDialog::confirmKeyPressed()) {
        commitEntry();
        close = true;
    }
}

void FrequencySelect::drawBandPage(bool& close, float totalWidth) {
    ImVec2 sp = ImGui::GetStyle().ItemSpacing;
    ImVec2 cancelSz(totalWidth, style::dp(42.0f));

    // Resolve the selected plan with the same fallback as bandplanmenu::init().
    auto it = bandplan::bandplans.find(planName);
    if (it == bandplan::bandplans.end()) { it = bandplan::bandplans.find("General"); }
    if (it == bandplan::bandplans.end() && !bandplan::bandplans.empty()) { it = bandplan::bandplans.begin(); }
    if (it == bandplan::bandplans.end()) {
        ImGui::TextDisabled("No band plan loaded");
        if (ImGui::Button("Cancel##sdrpp_band_cancel", cancelSz)) { close = true; }
        return;
    }
    const bandplan::BandPlan_t& plan = it->second;

    // Bands within the source tuning range (minFreq/maxFreq are display-domain,
    // same as the band plan), tagged with their category bucket.
    struct BandEntry {
        const bandplan::Band_t* band;
        const char* cat;
    };
    std::vector<BandEntry> avail;
    for (const auto& b : plan.bands) {
        if (limitFreq && (b.end < (double)minFreq || b.start > (double)maxFreq)) { continue; }
        avail.push_back({ &b, bandCategory(b.type) });
    }

    // Category row: only non-empty buckets, plus All. A persisted category that
    // vanished (plan or tuning range changed) falls back to All for display.
    static const char* buckets[] = { "Ham", "Bcast", "Air", "Marine", "Util" };
    bool present[5] = {};
    for (const auto& e : avail) {
        for (int i = 0; i < 5; i++) {
            if (!strcmp(e.cat, buckets[i])) { present[i] = true; break; }
        }
    }
    std::string effective = "All";
    for (int i = 0; i < 5; i++) {
        if (present[i] && category == buckets[i]) { effective = category; }
    }
    std::string newCategory;
    for (int i = 0; i < 5; i++) {
        if (!present[i]) { continue; }
        if (segButton(buckets[i], effective == buckets[i], ImVec2(0, 0))) { newCategory = buckets[i]; }
        ImGui::SameLine();
    }
    if (segButton("All", effective == "All", ImVec2(0, 0))) { newCategory = "All"; }
    if (!newCategory.empty() && newCategory != category) {
        category = newCategory;
        effective = newCategory;
        core::configManager.acquire();
        core::configManager.conf["freqEntryCategory"] = category;
        core::configManager.release(true);
    }
    ImGui::Spacing();

    std::vector<const bandplan::Band_t*> shown;
    for (const auto& e : avail) {
        if (effective == "All" || effective == e.cat) { shown.push_back(e.band); }
    }

    // OpenPopup must run at this (modal) scope, not inside the grid child, or
    // its popup ID won't match the BeginPopup below. The long-press detector
    // fires inside the child, so it only sets this flag.
    bool openRegPopup = false;
    if (shown.empty()) {
        ImGui::TextDisabled("No bands in the tuning range");
    }
    else {
        // 4-column grid of band keys in a child capped at ~4.5 rows; the half
        // row hints that the grid scrolls.
        float keyW = (totalWidth - 3.0f * sp.x) / 4.0f;
        float keyH = style::dp(52.0f);
        int rows = ((int)shown.size() + 3) / 4;
        bool scrolls = rows > 4;
        float gridH = scrolls ? 4.5f * (keyH + sp.y) : rows * (keyH + sp.y) - sp.y;
        float childW = totalWidth + (scrolls ? ImGui::GetStyle().ScrollbarSize : 0.0f);
        ImGuiIO& io = ImGui::GetIO();
        ImVec2 mousePos = ImGui::GetMousePos();
        ImGui::BeginChild("##sdrpp_band_grid", ImVec2(childW, gridH), false);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImU32 mainCol = ImGui::GetColorU32(ImGuiCol_Text);
        ImU32 subCol = ImGui::GetColorU32(ImGuiCol_Text, 0.75f);
        char id[32];
        for (int i = 0; i < (int)shown.size(); i++) {
            const bandplan::Band_t& b = *shown[i];
            ImGui::SetCursorPos(ImVec2((i % 4) * (keyW + sp.x), (i / 4) * (keyH + sp.y)));
            snprintf(id, sizeof(id), "##sdrpp_band_%d", i);
            bool clicked = ImGui::Button(id, ImVec2(keyW, keyH));
            // A quick tap recalls the newest register; a motionless hold opens
            // the band's register list. Stepping waits for release so the two
            // can't both fire (same idiom as the digit long-press).
            if (ImGui::IsItemActivated()) {
                pressBand = i;
                bandLongPressDone = false;
            }
            if (pressBand == i && ImGui::IsItemActive() && !bandLongPressDone) {
                float slop = 10.0f * style::uiScale;
                float dx = mousePos.x - io.MouseClickedPos[ImGuiMouseButton_Left].x;
                float dy = mousePos.y - io.MouseClickedPos[ImGuiMouseButton_Left].y;
                if ((dx * dx) + (dy * dy) <= (slop * slop) && io.MouseDownDuration[ImGuiMouseButton_Left] >= 0.5f) {
                    bandLongPressDone = true;
                    regPopupBand = &b;
                    openRegPopup = true;
#ifdef __ANDROID__
                    backend::hapticTick();
#endif
                }
            }
            if (clicked && !bandLongPressDone) {
                selectBand(b, plan);
                close = true;
            }
            ImVec2 bmin = ImGui::GetItemRectMin();
            ImVec2 bmax = ImGui::GetItemRectMax();
            float cx = (bmin.x + bmax.x) / 2.0f;
            float maxW = keyW - style::dp(8.0f);
            std::string main = mhzLabel(b.start);
            std::string sub = wavelengthToken(b.name);
            if (sub.empty() && strcmp(bandCategory(b.type), "Ham")) { sub = b.name; }
            dl->PushClipRect(bmin, bmax, true);
            if (sub.empty()) {
                centeredLabel(dl, style::bigFont, style::dp(22.0f), ImVec2(cx, (bmin.y + bmax.y) / 2.0f), maxW, mainCol, main.c_str());
            }
            else {
                centeredLabel(dl, style::bigFont, style::dp(22.0f), ImVec2(cx, bmin.y + keyH * 0.36f), maxW, mainCol, main.c_str());
                centeredLabel(dl, style::baseFont, style::dp(12.0f), ImVec2(cx, bmin.y + keyH * 0.76f), maxW, subCol, sub.c_str());
            }
            dl->PopClipRect();
        }
        ImGui::EndChild();
    }

    // Register list for a long-pressed band key (IC-705: touch the band key for
    // 1 second to display the Band Stacking Register contents).
    if (openRegPopup) { ImGui::OpenPopup("##sdrpp_band_registers"); }
    if (ImGui::BeginPopup("##sdrpp_band_registers")) {
        if (regPopupBand) {
            const bandplan::Band_t& b = *regPopupBand;
            ImGui::TextDisabled("%s", b.name.c_str());
            core::configManager.acquire();
            std::vector<BandRegister> regs = readBandRegisters(core::configManager.conf["bandMemory"], b);
            core::configManager.release();
            if (regs.empty()) {
                ImGui::TextDisabled("No stored registers");
            }
            char lbl[64];
            for (int k = 0; k < (int)regs.size(); k++) {
                snprintf(lbl, sizeof(lbl), "%d:  %s MHz  %s##sdrpp_reg_%d",
                         k + 1, mhzExact(regs[k].freq).c_str(), radioModeName(regs[k].mode), k);
                if (ImGui::Button(lbl, ImVec2(style::dp(150.0f), 0))) {
                    selectBand(b, plan, true, regs[k].freq, regs[k].mode);
                    close = true;
                    ImGui::CloseCurrentPopup();
                }
            }
        }
        ImGui::EndPopup();
    }

    if (ImGui::Button("Cancel##sdrpp_band_cancel", cancelSz)) { close = true; }
}

void FrequencySelect::selectBand(const bandplan::Band_t& band, const bandplan::BandPlan_t& plan,
                                 bool haveExplicit, double explicitFreq, int explicitMode) {
    std::string vfoName = gui::waterfall.selectedVFO;
    bool isRadio = !vfoName.empty() && core::modComManager.interfaceExists(vfoName)
                && core::modComManager.getModuleName(vfoName) == "radio";
    int curMode = -1;
    if (isRadio) {
        core::modComManager.callInterface(vfoName, RADIO_IFACE_CMD_GET_MODE, NULL, &curMode);
    }

    double targetFreq = haveExplicit ? explicitFreq : 0.0;
    int targetMode = haveExplicit ? explicitMode : -1;

    core::configManager.acquire();
    auto& mem = core::configManager.conf["bandMemory"];
    // Push the frequency/mode of the band being left onto its register stack.
    // Names repeat across plans (and within: e.g. "Shortwave Broadcast"
    // segments), so the first containing band wins and reads are revalidated by
    // containment.
    for (const auto& b : plan.bands) {
        if ((double)frequency >= b.start && (double)frequency <= b.end) {
            pushBandRegister(mem, b.name, (double)frequency, curMode);
            break;
        }
    }
    // A plain tap recalls the newest register; an explicit pick already has its
    // target and skips this.
    if (!haveExplicit) {
        std::vector<BandRegister> regs = readBandRegisters(mem, band);
        if (!regs.empty()) {
            targetFreq = regs[0].freq;
            targetMode = regs[0].mode;
        }
    }
    core::configManager.release(true);

    if (targetFreq <= 0.0) {
        targetFreq = (band.defFreq > 0.0) ? band.defFreq
                                          : round((band.start + band.end) / 2.0 / 1000.0) * 1000.0;
    }
    if (targetMode < 0) {
        targetMode = radioModeFromString(band.defMode);
        if (targetMode < 0) { targetMode = heuristicRadioMode(band); }
    }

    setFrequency((int64_t)round(targetFreq));
    frequencyChanged = true;
    if (isRadio && targetMode >= 0) {
        core::modComManager.callInterface(vfoName, RADIO_IFACE_CMD_SET_MODE, &targetMode, NULL);
    }
    // Channelized bands set the VFO snap after the mode change, which would
    // otherwise reset the snap to the mode default.
    if (band.chan > 0.0 && !vfoName.empty()) {
        auto vit = gui::waterfall.vfos.find(vfoName);
        if (vit != gui::waterfall.vfos.end() && vit->second) { vit->second->setSnapInterval(band.chan); }
    }
#ifdef __ANDROID__
    backend::hapticTick();
#endif
}

void FrequencySelect::setFrequency(int64_t freq) {
    freq = std::max<int64_t>(0, freq);
    int i = 11;
    for (uint64_t f = freq; i >= 0; i--) {
        digits[i] = f % 10;
        f -= digits[i];
        f /= 10;
    }
    frequency = freq;
}
