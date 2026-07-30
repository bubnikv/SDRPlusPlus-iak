#pragma once

#include <algorithm>
#include <cmath>
#include <imgui.h>
#include <gui/style.h>
#include <gui/widgets/simple_widgets.h>
#include <gui/widgets/toggle_style.h>

// Single-choice segmented control: one rounded pill split into abutting
// segments, in the style of the Android/Material segmented button. Use it for a
// handful of mutually exclusive views or filters sharing one row;
// doToggleButtonGrid() is the choice when there are enough options to wrap.
//
// The whole control is one ImGui item, and the segment is picked from the mouse
// x within it. Per-segment buttons cannot give this look: IsMouseHoveringRect()
// grows every hit box by TouchExtraPadding, so segments that touch would have
// overlapping hit boxes -- which is why doToggleButtonGrid() is obliged to keep
// a gap between its buttons, and a gap is exactly what this control must not
// have. One item also means the touch slop grows the control instead of each
// segment, and that a press latches its segment: sliding a finger onto a
// neighbour before release cannot change the answer.
//
// The latched segment takes the shared selected-toggle look (toggle_style.h)
// rather than ImGuiCol_ButtonActive, so it reads as latched on every theme.
//
// Returns true when `selected` changed.
inline bool doSegmentedControl(const char* strId, int& selected, const char* const* labels, int count, ImVec2 size = ImVec2(0.0f, 0.0f)) {
    if (count <= 0) { return false; }
    const ImGuiStyle& s = ImGui::GetStyle();
    if (size.x <= 0.0f) { size.x = ImGui::GetContentRegionAvail().x; }
    if (size.y <= 0.0f) { size.y = ImGui::GetFrameHeight(); }
    // InvisibleButton asserts on a zero extent, which a fully collapsed parent
    // can produce.
    if (size.x <= 0.0f || size.y <= 0.0f) { return false; }
    selected = std::clamp(selected, 0, count - 1);

    const ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::PushID(strId);
    const ImGuiID pressKey = ImGui::GetID("##seg");
    const ImGuiID animKey = ImGui::GetID("##seg_anim");
    const bool clicked = ImGui::InvisibleButton("##seg", size);
    ImGui::PopID();
    const bool hovered = ImGui::IsItemHovered();
    const bool held = ImGui::IsItemActive();

    // Segment under a screen x, clamped: the touch slop puts the pointer
    // slightly outside the control often enough to matter.
    const auto segAt = [&](float x) {
        return std::clamp((int)std::floor((x - p.x) * (float)count / size.x), 0, count - 1);
    };

    ImGuiIO& io = ImGui::GetIO();
    ImGuiStorage* st = ImGui::GetStateStorage();
    int* pressSeg = st->GetIntRef(pressKey, -1);
    if (ImGui::IsItemActivated()) {
        // Keyboard/gamepad activation has no meaningful click position.
        *pressSeg = ImGui::IsMouseDown(ImGuiMouseButton_Left)
            ? segAt(io.MouseClickedPos[ImGuiMouseButton_Left].x)
            : selected;
    }

    bool changed = false;
    if (clicked) {
        const int idx = (*pressSeg >= 0 && *pressSeg < count) ? *pressSeg : segAt(io.MousePos.x);
        *pressSeg = -1;
        if (idx != selected) {
            selected = idx;
            changed = true;
        }
    }
    const int hoverSeg = hovered ? segAt(io.MousePos.x) : -1;

    // Segment edges, from cumulative rounded slot boundaries so the segments
    // tile the pill exactly and every edge lands on a whole pixel.
    const auto edge = [&](int i) {
        return p.x + std::round(size.x * (float)i / (float)count);
    };

    // The thumb slides to the newly selected segment over ~120 ms, frame rate
    // independent, and snaps once it is within a hundredth of a segment so the
    // resting state is the pixel-exact one above. Drop this block and the two
    // `anim` uses below for an instant thumb.
    float* anim = st->GetFloatRef(animKey, (float)selected);
    *anim = std::clamp(*anim, 0.0f, (float)(count - 1));
    const bool settled = std::fabs((float)selected - *anim) < 0.01f;
    if (settled) { *anim = (float)selected; }
    else { *anim += ((float)selected - *anim) * (1.0f - std::exp(-io.DeltaTime / 0.045f)); }

    float thumbX0, thumbX1;
    if (settled) {
        thumbX0 = edge(selected);
        thumbX1 = edge(selected + 1);
    }
    else {
        thumbX0 = p.x + size.x * (*anim) / (float)count;
        thumbX1 = p.x + size.x * (*anim + 1.0f) / (float)count;
    }

    // Only the pill's own ends are round; every internal edge is square, which
    // is what makes the segments read as one control. Derived from where the
    // thumb currently is, so a mid-slide thumb keeps square ends.
    const auto capFlags = [&](float x0, float x1) {
        ImDrawFlags f = 0;
        if (x0 <= p.x + 0.5f) { f |= ImDrawFlags_RoundCornersTopLeft | ImDrawFlags_RoundCornersBottomLeft; }
        if (x1 >= p.x + size.x - 0.5f) { f |= ImDrawFlags_RoundCornersTopRight | ImDrawFlags_RoundCornersBottomRight; }
        // Not zero: ImDrawFlags treats no corner bits as "all corners".
        return f ? f : ImDrawFlags_RoundCornersNone;
    };

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const style::SelectedToggleColors c = style::selectedToggleColors();
    const ImVec2 pmax(p.x + size.x, p.y + size.y);
    const float rounding = std::min(size.y * 0.5f, style::dp(20.0f));
    const float line = style::lineWidth();
    // Outline and dividers come from Text, not ImGuiCol_Border: several themes
    // (android dark among them) set Border fully transparent, which would erase
    // the segment boundaries along with it.
    const ImU32 lineCol = ImGui::GetColorU32(ImGuiCol_Text, 0.20f);

    dl->AddRectFilled(p, pmax, ImGui::GetColorU32(ImGuiCol_Button), rounding);

    if (hoverSeg >= 0 && hoverSeg != selected) {
        dl->AddRectFilled(
            ImVec2(edge(hoverSeg), p.y),
            ImVec2(edge(hoverSeg + 1), pmax.y),
            ImGui::GetColorU32(held ? ImGuiCol_ButtonActive : ImGuiCol_ButtonHovered),
            rounding,
            capFlags(edge(hoverSeg), edge(hoverSeg + 1))
        );
    }

    const ImDrawFlags thumbFlags = capFlags(thumbX0, thumbX1);
    const ImVec4& thumbCol = (hoverSeg == selected) ? (held ? c.active : c.hovered) : c.bg;
    dl->AddRectFilled(ImVec2(thumbX0, p.y), ImVec2(thumbX1, pmax.y), ImGui::GetColorU32(thumbCol), rounding, thumbFlags);
    dl->AddRect(
        ImVec2(thumbX0 + line, p.y + line),
        ImVec2(thumbX1 - line, pmax.y - line),
        ImGui::GetColorU32(c.stroke),
        std::max(0.0f, rounding - line),
        thumbFlags,
        line
    );

    // Dividers, except where the thumb already supplies an edge.
    for (int i = 1; i < count; i++) {
        const float x = edge(i);
        if (x >= thumbX0 - 1.0f && x <= thumbX1 + 1.0f) { continue; }
        dl->AddLine(ImVec2(x, p.y + line), ImVec2(x, pmax.y - line), lineCol, line);
    }

    dl->AddRect(p, pmax, lineCol, rounding, 0, line);

    for (int i = 0; i < count; i++) {
        const float x0 = edge(i);
        const float x1 = edge(i + 1);
        const float cx = (x0 + x1) * 0.5f;
        // Follow the thumb rather than `selected`, so the inverted label
        // arrives with the fill instead of ahead of it.
        const bool onThumb = (cx >= thumbX0 && cx <= thumbX1);
        drawCenteredLabel(
            dl,
            ImGui::GetFont(),
            ImGui::GetFontSize(),
            ImVec2(cx, (p.y + pmax.y) * 0.5f),
            (x1 - x0) - 2.0f * s.FramePadding.x,
            onThumb ? ImGui::GetColorU32(c.content) : ImGui::GetColorU32(ImGuiCol_Text),
            labels[i]
        );
    }

    return changed;
}
