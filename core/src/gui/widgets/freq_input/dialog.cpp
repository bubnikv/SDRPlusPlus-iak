#include <gui/widgets/freq_input.h>
#include <gui/widgets/popup_dialog.h>
#include <gui/widgets/simple_widgets.h>
#include <gui/style.h>
#include <config.h>
#include <core.h>

namespace freq_input {

    Metrics Metrics::compute() {
        Metrics m;
        m.keySize = ImVec2(style::dp(56.0f), style::dp(42.0f));
        m.funcWidth = style::dp(84.0f);
        // Four columns, three gaps: the three digit columns plus the function
        // column. The band grid divides this same width into four keys.
        m.totalWidth = 3.0f * m.keySize.x + 3.0f * ImGui::GetStyle().ItemSpacing.x + m.funcWidth;
        return m;
    }

    Outcome Dialog::draw(const Context& ctx) {
        Outcome out;

        if (requestOpen) {
            requestOpen = false;
            keypad.onOpen();
            bands.onOpen();
            // Last-used page.
            core::configManager.acquire();
            page = (core::configManager.conf.value("freqEntryPage", "keypad") == "band") ? 0 : 1;
            core::configManager.release();
            ImGui::OpenPopup("F-INP##sdrpp_freq_keypad");
        }

        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        if (!ImGui::BeginPopupModal("F-INP##sdrpp_freq_keypad", NULL, ImGuiWindowFlags_AlwaysAutoResize)) { return out; }

        Metrics m = Metrics::compute();

        // Page toggle, sized to the keypad grid width so the popup width matches
        // on both pages.
        float halfWidth = (m.totalWidth - ImGui::GetStyle().ItemSpacing.x) / 2.0f;
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

        out = (page == 0) ? bands.draw(ctx, m) : keypad.draw(ctx, m);

        // Escape closes from either page, so it is handled here rather than
        // twice.
        if (PopupDialog::cancelKeyPressed()) { out.close = true; }
        if (out.close) { ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
        return out;
    }

}
