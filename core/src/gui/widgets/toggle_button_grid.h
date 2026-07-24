#pragma once

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>
#include <imgui.h>
#include <gui/style.h>
#include <gui/widgets/toggle_style.h>

// Compact single-choice bank of toggle buttons (PowerSDR/Thetis-style mode
// grid). Fills the available width, wrapping into as many balanced rows as
// needed for the widest label to fit; the selected button gets the shared
// latched-toggle look (see toggle_style.h), so the active state does not depend
// on theme ButtonActive contrast. Buttons within the bank use
// tighter-than-normal spacing so the group reads as one control; the gap is
// kept at >= 2x TouchExtraPadding so adjacent hit boxes never overlap in the
// touch style.
// Returns true when selected changed.
inline bool doToggleButtonGrid(const std::string& id, int& selected, const std::vector<std::string>& labels) {
    const int count = (int)labels.size();
    const ImGuiStyle& s = ImGui::GetStyle();
    const style::SelectedToggleColors selectedCols = style::selectedToggleColors();

    const float gapX = std::max(style::dp(3.0f), 2.0f * s.TouchExtraPadding.x);
    const float gapY = std::max(style::dp(3.0f), 2.0f * s.TouchExtraPadding.y);
    const float avail = ImGui::GetContentRegionAvail().x;

    float maxLabel = 0.0f;
    for (const std::string& label : labels) {
        maxLabel = std::max(maxLabel, ImGui::CalcTextSize(label.c_str()).x);
    }
    const float minButton = maxLabel + 2.0f * s.FramePadding.x;

    const int perRow = std::clamp((int)((avail + gapX) / (minButton + gapX)), 1, count);
    const int rows = (count + perRow - 1) / perRow;
    const int base = count / rows;
    const int extra = count % rows;

    bool changed = false;
    ImGui::PushID(id.c_str());
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(gapX, gapY));
    ImGui::BeginGroup();
    int idx = 0;
    for (int r = 0; r < rows; r++) {
        const int n = base + ((r < extra) ? 1 : 0);
        // Distribute the row width by rounding cumulative slot edges so the
        // row spans exactly avail regardless of fractional button widths.
        const float slot = (avail + gapX) / (float)n;
        for (int c = 0; c < n; c++, idx++) {
            if (c) { ImGui::SameLine(); }
            const float w = std::round(slot * (float)(c + 1)) - std::round(slot * (float)c) - gapX;
            const bool sel = (idx == selected);
            if (sel) { style::pushSelectedToggle(selectedCols); }
            if (ImGui::Button(labels[idx].c_str(), ImVec2(w, 0)) && !sel) {
                selected = idx;
                changed = true;
            }
            if (sel) {
                style::drawSelectedToggleStroke(selectedCols);
                style::popSelectedToggle();
            }
        }
    }
    ImGui::EndGroup();
    ImGui::PopStyleVar();
    ImGui::PopID();
    return changed;
}
