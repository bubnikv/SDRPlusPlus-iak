#include <gui/widgets/freq_input.h>
#include <gui/widgets/popup_dialog.h>
#include <gui/widgets/segmented_control.h>
#include <gui/style.h>
#include <config.h>
#include <core.h>
#include <algorithm>
#include <cmath>

namespace freq_input {

    // Columns of `minKey` that fit in `avail`, clamped to a sane band.
    static int colsThatFit(float avail, float minKey, float spacing, int lo, int hi) {
        const int fit = (int)std::floor((avail + spacing) / (minKey + spacing));
        return std::clamp(fit, lo, hi);
    }

    int Metrics::rowsThatFit(float avail, float keyH) {
        const float sp = ImGui::GetStyle().ItemSpacing.y;
        return std::max(2, (int)std::floor((avail + sp) / (keyH + sp)));
    }

    float Metrics::gridHeight(int rowsNeeded, int rowsFit, float keyH) {
        const float sp = ImGui::GetStyle().ItemSpacing.y;
        const float rows = (rowsNeeded <= rowsFit) ? (float)rowsNeeded
                                                   : ((float)rowsFit - 0.5f);
        return rows * (keyH + sp) - sp;
    }

    Metrics Metrics::compute() {
        Metrics m;
        const ImGuiStyle& s = ImGui::GetStyle();
        const ImVec2 sp = s.ItemSpacing;
        const ImVec2 viewport = ImGui::GetMainViewport()->Size;

        // Keep the dialog off the screen edges, and discount the window padding
        // it will sit inside.
        const float margin = style::dp(8.0f);
        const float availW = viewport.x - 2.0f * (margin + s.WindowPadding.x);
        const float availH = viewport.y - 2.0f * (margin + s.WindowPadding.y);

        // Material's 48 dp minimum touch target. A mouse is precise enough that
        // the frame height serves on desktop.
        m.rowHeight = style::touchStyle
            ? std::max(style::dp(48.0f), ImGui::GetFrameHeight())
            : ImGui::GetFrameHeight();
        m.toggleHeight = std::max(style::dp(34.0f), m.rowHeight);

        // Keypad. The digit keys give width back only when the viewport cannot
        // hold them at all, which takes something narrower than a phone in
        // portrait.
        m.funcWidth = style::dp(84.0f);
        const float keyW = std::clamp((availW - m.funcWidth - 3.0f * sp.x) / 3.0f,
                                      style::dp(40.0f), style::dp(56.0f));
        m.keySize = ImVec2(keyW, std::max(style::dp(42.0f), m.rowHeight));
        m.keypadWidth = 3.0f * m.keySize.x + 3.0f * sp.x + m.funcWidth;

        // Content width: the keypad block is the floor, the viewport drives it,
        // and the cap stops a 4K monitor from producing a wall of keys.
        m.totalWidth = std::clamp(availW, m.keypadWidth, style::dp(640.0f));
        m.pageHeight = availH - m.toggleHeight - 2.0f * sp.y;

        // Put the readout beside the keys whenever there is a column's worth of
        // room next to them. That is one test for two cases: a phone in
        // landscape and a desktop window are both far wider than the keypad,
        // and stacking is what does not fit a landscape phone vertically.
        m.wideKeypad = (m.totalWidth - m.keypadWidth - sp.x) >= style::dp(200.0f);

        m.gridWidth = m.totalWidth - s.ScrollbarSize;
        m.bandCols = colsThatFit(m.gridWidth, style::dp(63.0f), sp.x, 4, 10);
        m.spectrumCols = colsThatFit(m.gridWidth, style::dp(87.0f), sp.x, 3, 8);
        m.bandKeyHeight = std::max(style::dp(52.0f), m.rowHeight);
        m.spectrumKeyHeight = std::max(style::dp(54.0f), m.rowHeight);
        return m;
    }

    // Close glyph. Roboto-Medium ships no multiplication sign in our glyph
    // ranges, so the cross is drawn by hand, as the backspace key's icon is.
    static void drawCloseIcon() {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 bmin = ImGui::GetItemRectMin();
        const ImVec2 bmax = ImGui::GetItemRectMax();
        const ImVec2 ctr((bmin.x + bmax.x) / 2.0f, (bmin.y + bmax.y) / 2.0f);
        const float r = style::dp(6.0f);
        const float th = style::lineWidth(1.5f);
        const ImU32 col = ImGui::GetColorU32(ImGuiCol_Text);
        dl->AddLine(ImVec2(ctr.x - r, ctr.y - r), ImVec2(ctr.x + r, ctr.y + r), col, th);
        dl->AddLine(ImVec2(ctr.x - r, ctr.y + r), ImVec2(ctr.x + r, ctr.y - r), col, th);
    }

    Outcome Dialog::draw(const Context& ctx) {
        Outcome out;

        if (requestOpen) {
            requestOpen = false;
            keypad.onOpen();
            bands.onOpen();
            spectrum.onOpen();
            // Last-used page.
            core::configManager.acquire();
            const std::string storedPage =
                core::configManager.conf.value("freqEntryPage", "keypad");
            if (storedPage == "band") { page = 0; }
            else if (storedPage == "spectrum") { page = 1; }
            else { page = 2; }
            core::configManager.release();
            ImGui::OpenPopup("F-INP##sdrpp_freq_keypad");
        }

        const ImVec2 viewport = ImGui::GetMainViewport()->Size;
        if (viewport.x != lastViewport.x || viewport.y != lastViewport.y) {
            lastViewport = viewport;
            recenter = true;
        }
        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),
                                recenter ? ImGuiCond_Always : ImGuiCond_Appearing,
                                ImVec2(0.5f, 0.5f));
        recenter = false;
        // No title bar: it would read "F-INP" on all three pages, contradicting
        // the segment below it on two of them, and its height is the scarcest
        // thing this dialog has on a phone in landscape. The page toggle names
        // the dialog.
        if (!ImGui::BeginPopupModal("F-INP##sdrpp_freq_keypad", NULL,
                                    ImGuiWindowFlags_AlwaysAutoResize |
                                    ImGuiWindowFlags_NoTitleBar))
        {
            return out;
        }

        Metrics m = Metrics::compute();
        const float sp = ImGui::GetStyle().ItemSpacing.x;

        // Page toggle plus the dialog's one dismiss control, in the same place
        // on every page -- the pages below no longer carry a Cancel of their
        // own. `page` is the segment index by construction.
        static const char* pageLabels[] = { "BAND", "SPECTRUM", "F-INP" };
        const float closeW = m.toggleHeight;
        if (doSegmentedControl("##sdrpp_finp_page", page, pageLabels, 3,
                               ImVec2(m.totalWidth - closeW - sp, m.toggleHeight)))
        {
            core::configManager.acquire();
            core::configManager.conf["freqEntryPage"] =
                (page == 0) ? "band" :
                (page == 1) ? "spectrum" : "keypad";
            core::configManager.release(true);
        }
        ImGui::SameLine();
        const bool closePressed =
            ImGui::Button("##sdrpp_finp_close", ImVec2(closeW, m.toggleHeight));
        drawCloseIcon();
        ImGui::Spacing();

        if (page == 0) { out = bands.draw(ctx, m); }
        else if (page == 1) { out = spectrum.draw(ctx, m); }
        else { out = keypad.draw(ctx, m); }

        // Escape closes from any page, so it is handled here rather than three
        // times.
        if (closePressed || PopupDialog::cancelKeyPressed()) { out.close = true; }
        if (out.close) { ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
        return out;
    }

}
