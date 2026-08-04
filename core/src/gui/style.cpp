#include <gui/style.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <config.h>
#include <utils/flog.h>
#include <filesystem>
#include <cmath>

namespace style {
    ImFont* baseFont;
    ImFont* bigFont;
    ImFont* hugeFont;
    ImVector<ImWchar> baseRanges;
    ImVector<ImWchar> bigRanges;
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

        // Create big font range
        ImFontGlyphRangesBuilder bigBuilder;
        const ImWchar bigRange[] = { '.', '9', 0 };
        bigBuilder.AddRanges(bigRange);
        bigBuilder.BuildRanges(&bigRanges);

        // Create huge font range
        ImFontGlyphRangesBuilder hugeBuilder;
        hugeBuilder.AddText("SDR++ iak");
        hugeBuilder.BuildRanges(&hugeRanges);
        
        // Add bigger fonts for frequency select and title. Rasterize at
        // whole-pixel sizes: fractional scales (1.25x, 1.75x, ...) would
        // otherwise yield fractional glyph metrics that leak into every layout
        // computed from CalcTextSize().
        baseFont = fonts->AddFontFromFileTTF(((std::string)(resDir + "/fonts/Roboto-Medium.ttf")).c_str(), roundf(16.0f * uiScale), NULL, baseRanges.Data);
        bigFont = fonts->AddFontFromFileTTF(((std::string)(resDir + "/fonts/Roboto-Medium.ttf")).c_str(), roundf(45.0f * uiScale), NULL, bigRanges.Data);
        hugeFont = fonts->AddFontFromFileTTF(((std::string)(resDir + "/fonts/Roboto-Medium.ttf")).c_str(), roundf(128.0f * uiScale), NULL, hugeRanges.Data);

        return true;
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

    void migrateLogicalDimension(ConfigManager::Transaction& txn, const char* valueKey, const char* markerKey, float minLogical, const std::function<bool(float)>& valueLooksPhysical) {
        if (txn.value(markerKey, false)) { return; }

        float value = txn.value(valueKey, minLogical);
        if (valueLooksPhysical(value)) {
            value = (float)unscale(value);
        }
        txn.set(valueKey, (int)std::round(std::max(value, minLogical)));
        txn.set(markerKey, true);
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
