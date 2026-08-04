#pragma once
#include <imgui.h>
#include <string>
#include <config.h>
#include <module.h>
#include <functional>
#include <algorithm>
#include <cmath>
#include <stdint.h>

namespace style {
    SDRPP_EXPORT ImFont* baseFont;
    SDRPP_EXPORT ImFont* bigFont;
    SDRPP_EXPORT ImFont* hugeFont;
    SDRPP_EXPORT float uiScale;
    SDRPP_EXPORT bool touchStyle;

    void setUIScale(float scale);
    uint64_t scaleEpoch();

    inline float dp(float logical) {
        return logical * uiScale;
    }

    inline ImVec2 dp(float x, float y) {
        return ImVec2(dp(x), dp(y));
    }

    inline int scale(float logical) {
        return (int)std::round(dp(logical));
    }

    // DrawList stroke thickness: whole pixels only, so 1px lines stay crisp at
    // fractional UI scales instead of antialiasing across a half pixel.
    inline float lineWidth(float logical = 1.0f) {
        return (std::max)(1.0f, std::round(dp(logical)));
    }

    // Height of the main window's top-bar row: the footprint of its icon
    // buttons (30 dp glyph + 5 dp padding either side). Everything in that row
    // -- buttons, volume slider, frequency digits, level meter, logo -- centres
    // on this one height, so it lives here rather than being restated by each
    // widget with its own hand-tuned offset.
    inline float topBarRowHeight() {
        return (float)(scale(30.0f) + 2 * scale(5.0f));
    }

    // Y offset, from the top of the row, that vertically centres an item of
    // `itemHeight`. Whole pixels so glyphs and 1px rules stay crisp.
    inline float topBarRowOffset(float itemHeight) {
        return std::round((topBarRowHeight() - itemHeight) * 0.5f);
    }

    inline int scaleOrPhysical(float logicalOrPhysical, float physicalThresholdLogical) {
        if (uiScale > 1.0f && logicalOrPhysical >= dp(physicalThresholdLogical)) {
            return (int)std::round(logicalOrPhysical);
        }
        return scale(logicalOrPhysical);
    }

    inline int unscale(float physical) {
        return (int)std::round(physical / uiScale);
    }

    inline int rescale(int physical, float oldScale) {
        return (int)std::round((float)physical * uiScale / oldScale);
    }

    inline int clampSplit(float desired, float available, float minBefore, float minAfter) {
        float minBeforePx = dp(minBefore);
        float maxBeforePx = available - dp(minAfter);
        if (maxBeforePx < minBeforePx) {
            return (int)std::round((std::max)(0.0f, maxBeforePx));
        }
        return (int)std::round(std::clamp(desired, minBeforePx, maxBeforePx));
    }

    // Horizontal inset of full-row action buttons in menu panels, so their
    // silhouette differs from the edge-to-edge CollapsingHeader bars.
    float menuButtonInset();

    bool setDefaultStyle(std::string resDir);
    bool loadFonts(std::string resDir);
    void applyScaledStyle(const std::function<void()>& resetStyle);
    // Rewrites a dimension stored in physical pixels as logical units, once, and
    // leaves a marker key behind so it isn't converted twice. Takes the open
    // edit access rather than the document, since it both reads and writes.
    void migrateLogicalDimension(ConfigManager::EditAccess& configAccess, const char* valueKey, const char* markerKey, float minLogical, const std::function<bool(float)>& valueLooksPhysical);
    void beginDisabled();
    void endDisabled();
    void testtt();
}

namespace ImGui {
    void LeftLabel(const char* text);
    void FillWidth();
    void SetNextItemRemainingWidth();
    void LeftLabelFill(const char* text);
    // Full-row action button, inset from the panel edges so it is not
    // mistaken for a menu section header.
    bool ActionButton(const char* label);
    // Shift the cursor for an inset action-button row and return the row
    // width, e.g. for a BeginTable holding a split button group.
    float BeginActionRow();
}
