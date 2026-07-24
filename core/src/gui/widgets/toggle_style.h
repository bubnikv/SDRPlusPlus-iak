#pragma once

#include <algorithm>
#include <imgui.h>
#include <gui/style.h>

// Shared "this control is latched on" look, so every toggle in the app reads
// the same way.
//
// The fill is derived from the theme (WindowBg mixed toward Text) rather than
// taken from ImGuiCol_ButtonActive: ButtonActive is the colour a button flashes
// while it is being pressed, and in several themes it barely differs from
// ImGuiCol_Button, so a latched control drawn with it reads as "idle". Mixing
// against the window background instead guarantees contrast on any theme, and
// pairs with the background as the content colour (inverted) plus a thin inset
// stroke so the state survives even where fill and text are close.
namespace style {
    struct SelectedToggleColors {
        ImVec4 bg;      // ImGuiCol_Button while selected
        ImVec4 hovered; // ImGuiCol_ButtonHovered while selected
        ImVec4 active;  // ImGuiCol_ButtonActive while selected
        ImVec4 content; // text colour, or image tint, on top of the fill
        ImVec4 stroke;  // inset outline
    };

    inline SelectedToggleColors selectedToggleColors() {
        const ImGuiStyle& s = ImGui::GetStyle();
        auto mix = [](const ImVec4& a, const ImVec4& b, float t) {
            return ImVec4(
                a.x + (b.x - a.x) * t,
                a.y + (b.y - a.y) * t,
                a.z + (b.z - a.z) * t,
                a.w + (b.w - a.w) * t
            );
        };

        const ImVec4 windowBg = s.Colors[ImGuiCol_WindowBg];
        const ImVec4 textCol = s.Colors[ImGuiCol_Text];
        // style::beginDisabled() fades Button and Text together; detecting it
        // from the live colours keeps this usable inside a disabled block
        // without threading a flag through every caller.
        const bool disabled = s.Colors[ImGuiCol_Button].w <= 0.25f && textCol.w <= 0.75f;
        const float alpha = disabled ? std::max(0.25f, s.Colors[ImGuiCol_Button].w) : 0.95f;

        SelectedToggleColors c;
        c.bg = mix(windowBg, textCol, 0.60f);
        c.bg.w = alpha;
        c.hovered = mix(c.bg, textCol, 0.10f);
        c.hovered.w = alpha;
        c.active = mix(c.bg, textCol, 0.18f);
        c.active.w = alpha;
        c.content = windowBg;
        c.content.w = disabled ? textCol.w : 1.0f;
        c.stroke = c.content;
        c.stroke.w *= disabled ? 0.35f : 0.55f;
        return c;
    }

    // Frame colours + Text. Callers that tint an image instead of drawing text
    // (image buttons) use c.content directly and can still use this for the
    // frame. Pair with popSelectedToggle().
    inline void pushSelectedToggle(const SelectedToggleColors& c) {
        ImGui::PushStyleColor(ImGuiCol_Button, c.bg);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, c.hovered);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, c.active);
        ImGui::PushStyleColor(ImGuiCol_Text, c.content);
    }

    inline void popSelectedToggle() {
        ImGui::PopStyleColor(4);
    }

    // Inset outline around the item just submitted.
    inline void drawSelectedToggleStroke(const SelectedToggleColors& c) {
        const ImVec2 min = ImGui::GetItemRectMin();
        const ImVec2 max = ImGui::GetItemRectMax();
        const float inset = style::dp(1.0f);
        const float rounding = std::max(0.0f, ImGui::GetStyle().FrameRounding - inset);
        ImGui::GetWindowDrawList()->AddRect(
            ImVec2(min.x + inset, min.y + inset),
            ImVec2(max.x - inset, max.y - inset),
            ImGui::GetColorU32(c.stroke),
            rounding,
            0,
            style::lineWidth()
        );
    }
}
