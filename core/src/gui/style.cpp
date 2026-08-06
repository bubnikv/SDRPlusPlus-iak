#include <gui/style.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <config.h>
#include <utils/flog.h>
#include <filesystem>
#include <cmath>
#include <cstring>

namespace style {
    ImFont* baseFont;
    ImFont* labelFont;
    ImFont* bigFont;
    ImFont* hugeFont;
    ImVector<ImWchar> baseRanges;
    ImVector<ImWchar> hugeRanges;

#ifndef __ANDROID__
    float uiScale = 1.0f;
    bool touchStyle = false;
#else
    float uiScale = 3.0f;
    bool touchStyle = true;
#endif
    uint64_t _scaleEpoch = 0;

    void setUIScale(float scale) {
        if (std::fabs(uiScale - scale) < 0.0001f) { return; }
        uiScale = scale;
        _scaleEpoch++;
    }

    uint64_t scaleEpoch() {
        return _scaleEpoch;
    }

    bool loadFonts(std::string resDir) {
        ImFontAtlas* fonts = ImGui::GetIO().Fonts;
        if (!std::filesystem::is_directory(resDir)) {
            flog::error("Invalid resource directory: {0}", resDir);
            return false;
        }

        // BuildRanges appends to its output, so every rebuild -- one per UI
        // scale change -- would stack another copy of each range behind the
        // terminator the previous build already wrote.
        baseRanges.clear();
        hugeRanges.clear();

        // Create base font range
        ImFontGlyphRangesBuilder baseBuilder;
        baseBuilder.AddRanges(fonts->GetGlyphRangesDefault());
        baseBuilder.AddRanges(fonts->GetGlyphRangesCyrillic());
        // Extra glyphs from the RDS/EBU G0 repertoire that Roboto-Medium has but
        // the default ranges omit (Latin Extended-A, plus a few singletons).
        static const ImWchar latinExtA[] = { 0x0100, 0x017F, 0 };
        baseBuilder.AddRanges(latinExtA);
        baseBuilder.AddChar(0x03B1); // α
        baseBuilder.AddChar(0x03C0); // π
        baseBuilder.AddChar(0x2015); // ―
        baseBuilder.AddChar(0x2030); // ‰
        baseBuilder.AddChar(0x20AC); // €
        baseBuilder.BuildRanges(&baseRanges);

        // Ranges are inclusive [first, last] pairs terminated by 0, and go to
        // the atlas as they are: a builder is only worth its 8 KB bit vector
        // where several ranges have to be merged, as above and for the title.
        //
        // Printable ASCII spells every band selector label and mode name.
        // GetGlyphRangesDefault() would reach 0x00FF for another 96 glyphs, but
        // at this rasterization size that is 2 MB of atlas at uiScale 3 for
        // accented letters no label uses; fontFor() sends anything outside this
        // set to baseFont, which has them.
        static const ImWchar labelRange[] = { 0x0020, 0x007E, 0 };

        // Digits, '.' and '/', and nothing else: holding neither U+FFFD, '?'
        // nor space, this font's fallback glyph is its own last one, '9'. Any
        // text drawn with it that is not a number therefore renders as a
        // plausible different number -- which is why drawCenteredLabel() sends
        // it through fontFor() rather than trusting the caller.
        static const ImWchar bigRange[] = { '.', '9', 0 };

        // Create huge font range
        ImFontGlyphRangesBuilder hugeBuilder;
        hugeBuilder.AddText("SDR++ iak");
        hugeBuilder.BuildRanges(&hugeRanges);
        
        // Add bigger fonts for grid keys, frequency select and title. Rasterize
        // at whole-pixel sizes: fractional scales (1.25x, 1.75x, ...) would
        // otherwise yield fractional glyph metrics that leak into every layout
        // computed from CalcTextSize(). Keep them in ascending size order --
        // fontFor() walks them in declaration order.
        const std::string fontPath = resDir + "/fonts/Roboto-Medium.ttf";
        // Three horizontal samples buy nothing at the label size and cost half
        // again as much atlas as two, which is what pays for its glyph set.
        ImFontConfig labelCfg;
        labelCfg.OversampleH = 2;
        baseFont = fonts->AddFontFromFileTTF(fontPath.c_str(), roundf(16.0f * uiScale), NULL, baseRanges.Data);
        labelFont = fonts->AddFontFromFileTTF(fontPath.c_str(), roundf(22.0f * uiScale), &labelCfg, labelRange);
        bigFont = fonts->AddFontFromFileTTF(fontPath.c_str(), roundf(45.0f * uiScale), NULL, bigRange);
        hugeFont = fonts->AddFontFromFileTTF(fontPath.c_str(), roundf(128.0f * uiScale), NULL, hugeRanges.Data);

        // Glyph sets are not a smooth cost: the atlas is one texture whose
        // height is rounded up to a power of two, so widening a range can
        // double it -- and at uiScale 3 it is already megapixels. Report the
        // result instead of leaving it to be found as an out-of-memory on a
        // phone. Building here rather than at first use is free; the backend
        // would build the same atlas a moment later.
        fonts->Build();
        flog::debug("Font atlas: {0}x{1} px at scale {2}", fonts->TexWidth, fonts->TexHeight, uiScale);

        return true;
    }

    bool fontCovers(ImFont* font, const char* text, const char* textEnd) {
        if (!font || !text) { return false; }
        if (!textEnd) { textEnd = text + strlen(text); }
        while (text < textEnd) {
            unsigned int c = (unsigned int)(unsigned char)*text;
            if (c < 0x80) {
                text++;
                // A line break is consumed by the text renderer itself and
                // never looked up, so no font is disqualified by carrying one.
                if (c == '\n') { continue; }
            }
            else {
                const int len = ImTextCharFromUtf8(&c, text, textEnd);
                if (len <= 0) { return false; }
                text += len;
                if (c == 0 || c > IM_UNICODE_CODEPOINT_MAX) { return false; }
            }
            if (!font->FindGlyphNoFallback((ImWchar)c)) { return false; }
        }
        return true;
    }

    ImFont* fontFor(const char* text, float sizePx, const char* textEnd) {
        ImFont* const candidates[] = { baseFont, labelFont, bigFont, hugeFont };
        for (ImFont* font : candidates) {
            // The rasterized size is rounded to whole pixels, so at a
            // fractional UI scale it can land just under the dp() request it is
            // meant to serve.
            if (!font || font->FontSize < sizePx - 0.5f) { continue; }
            if (fontCovers(font, text, textEnd)) { return font; }
        }
        // Nothing both large enough and complete: baseFont has by far the
        // widest repertoire, so magnifying it is the closest to right.
        return baseFont;
    }

    // Android-like touch overlay: rounded borderless surfaces and moderately
    // enlarged metrics — elements stay within ~25% of the desktop defaults
    // (frame height 22 -> 26 dp, row pitch 26 -> 31 dp). TouchExtraPadding
    // must not exceed half of ItemSpacing or adjacent hit boxes overlap.
    // Sizes only — theme colors are untouched, so it composes with any theme.
    static void applyTouchOverlay() {
        ImGuiStyle& s = ImGui::GetStyle();

        s.WindowPadding     = dp(10.0f, 10.0f);
        s.FramePadding      = dp(6.0f, 5.0f);
        s.ItemSpacing       = dp(10.0f, 5.0f);
        s.ItemInnerSpacing  = dp(5.0f, 5.0f);
        s.CellPadding       = dp(5.0f, 2.5f);
        s.ScrollbarSize     = dp(10.0f);
        s.GrabMinSize       = dp(12.5f);
        s.TouchExtraPadding = dp(2.0f, 2.0f);

        s.FrameRounding     = dp(8.0f);
        s.GrabRounding      = dp(8.0f);
        s.PopupRounding     = dp(12.0f);
        s.ChildRounding     = dp(8.0f);
        s.ScrollbarRounding = dp(8.0f);
        s.TabRounding       = dp(8.0f);

        s.WindowBorderSize  = 0.0f;
        s.ChildBorderSize   = 0.0f;
        s.PopupBorderSize   = 0.0f;
        s.FrameBorderSize   = 0.0f;
    }

    void applyScaledStyle(const std::function<void()>& resetStyle) {
        ImGui::GetStyle() = ImGuiStyle();
        resetStyle();
        ImGui::GetStyle().ScaleAllSizes(uiScale);
        if (touchStyle) { applyTouchOverlay(); }
    }

    void migrateLogicalDimension(ConfigManager::EditAccess& configAccess, const char* valueKey, const char* markerKey, float minLogical, const std::function<bool(float)>& valueLooksPhysical) {
        if (configAccess.value(markerKey, false)) { return; }

        float value = configAccess.value(valueKey, minLogical);
        if (valueLooksPhysical(value)) {
            value = (float)unscale(value);
        }
        configAccess.set(valueKey, (int)std::round(std::max(value, minLogical)));
        configAccess.set(markerKey, true);
    }

    float menuButtonInset() {
        return dp(touchStyle ? 16.0f : 10.0f);
    }

    void beginDisabled() {
        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
        auto& style = ImGui::GetStyle();
        ImVec4* colors = style.Colors;
        ImVec4 btnCol = colors[ImGuiCol_Button];
        ImVec4 frameCol = colors[ImGuiCol_FrameBg];
        ImVec4 textCol = colors[ImGuiCol_Text];
        btnCol.w = 0.15f;
        frameCol.w = 0.30f;
        textCol.w = 0.65f;
        ImGui::PushStyleColor(ImGuiCol_Button, btnCol);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, frameCol);
        ImGui::PushStyleColor(ImGuiCol_Text, textCol);
    }

    void endDisabled() {
        ImGui::PopItemFlag();
        ImGui::PopStyleColor(3);
    }
}

namespace ImGui {
    void LeftLabel(const char* text) {
        float vpos = ImGui::GetCursorPosY();
        ImGui::SetCursorPosY(vpos + GImGui->Style.FramePadding.y);
        ImGui::TextUnformatted(text);
        ImGui::SameLine();
        ImGui::SetCursorPosY(vpos);
    }

    void FillWidth() {
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    }

    void SetNextItemRemainingWidth() {
        FillWidth();
    }

    void LeftLabelFill(const char* text) {
        LeftLabel(text);
        FillWidth();
    }

    float BeginActionRow() {
        float inset = style::menuButtonInset();
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + inset);
        return std::max(1.0f, ImGui::GetContentRegionAvail().x - inset);
    }

    bool ActionButton(const char* label) {
        return ImGui::Button(label, ImVec2(BeginActionRow(), 0));
    }
}
