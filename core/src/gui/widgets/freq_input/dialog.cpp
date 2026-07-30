#include <gui/widgets/freq_input.h>
#include <gui/widgets/popup_dialog.h>
#include <gui/widgets/segmented_control.h>
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

        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        if (!ImGui::BeginPopupModal("F-INP##sdrpp_freq_keypad", NULL, ImGuiWindowFlags_AlwaysAutoResize)) { return out; }

        Metrics m = Metrics::compute();

        // Page toggle, sized to the keypad grid width so the popup width matches
        // on every page. `page` is the segment index by construction.
        static const char* pageLabels[] = { "BAND", "SPECTRUM", "F-INP" };
        if (doSegmentedControl("##sdrpp_finp_page", page, pageLabels, 3, ImVec2(m.totalWidth, style::dp(34.0f)))) {
            core::configManager.acquire();
            core::configManager.conf["freqEntryPage"] =
                (page == 0) ? "band" :
                (page == 1) ? "spectrum" : "keypad";
            core::configManager.release(true);
        }
        ImGui::Spacing();

        if (page == 0) { out = bands.draw(ctx, m); }
        else if (page == 1) { out = spectrum.draw(ctx, m); }
        else { out = keypad.draw(ctx, m); }

        // Escape closes from either page, so it is handled here rather than
        // twice.
        if (PopupDialog::cancelKeyPressed()) { out.close = true; }
        if (out.close) { ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
        return out;
    }

}
