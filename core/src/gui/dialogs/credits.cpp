#include <gui/dialogs/credits.h>
#include <imgui.h>
#include <gui/widgets/popup_dialog.h>
#include <gui/icons.h>
#include <gui/style.h>
#include <config.h>
#include <credits.h>
#include <version.h>

namespace credits {
    ImFont* bigFont;
    ImVec2 imageSize(128.0f, 128.0f);

    void init() {
        imageSize = style::dp(128.0f, 128.0f);
    }

    bool show() {
        // True once a touch/click began while the dialog was already open. Prevents the
        // click that opened the dialog (or one that started on the scrollbar) from closing it.
        static bool dismissArmed = false;

        bool open = true;
        imageSize = style::dp(128.0f, 128.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, style::dp(20.0f, 20.0f));
#ifdef __ANDROID__
        // Wide enough to grab with a finger when the dialog overflows a small screen
        ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, style::dp(25.0f));
#endif
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
        ImVec2 dispSize = ImGui::GetIO().DisplaySize;
        ImVec2 center = ImVec2(dispSize.x / 2.0f, dispSize.y / 2.0f);
        ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::OpenPopup("Credits");
        ImGui::BeginPopupModal("Credits", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove);

        ImGui::PushFont(style::hugeFont);
        ImGui::TextUnformatted("SDR++ jp      ");
        ImGui::PopFont();
        ImGui::SameLine();
        ImGui::Image(icons::LOGO, imageSize);
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Spacing();

        ImGui::TextUnformatted("Forked from Vojtech Bubnik (OK1IAK), original software by Alexandre Rouma (ON5RYZ) with the help of\n\n");

        ImGui::Columns(4, "CreditColumns", true);

        ImGui::TextUnformatted("Contributors");
        for (int i = 0; i < sdrpp_credits::contributorCount; i++) {
            ImGui::BulletText("%s", sdrpp_credits::contributors[i]);
        }

        ImGui::NextColumn();
        ImGui::TextUnformatted("Libraries");
        for (int i = 0; i < sdrpp_credits::libraryCount; i++) {
            ImGui::BulletText("%s", sdrpp_credits::libraries[i]);
        }

        ImGui::NextColumn();
        ImGui::TextUnformatted("Hardware Donators");
        for (int i = 0; i < sdrpp_credits::hardwareDonatorCount; i++) {
            ImGui::BulletText("%s", sdrpp_credits::hardwareDonators[i]);
        }

        ImGui::NextColumn();
        ImGui::TextUnformatted("Patrons");
        for (int i = 0; i < sdrpp_credits::patronCount; i++) {
            ImGui::BulletText("%s", sdrpp_credits::patrons[i]);
        }

        ImGui::Columns(1, "CreditColumnsEnd", true);

        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::TextUnformatted("SDR++ jp v" VERSION_STR " (Built at " __TIME__ ", " __DATE__ ")");

        ImGuiIO& io = ImGui::GetIO();
        float dragThreshold = std::max(io.MouseDragThreshold, style::dp(6.0f));

        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            // Presses landing on the scrollbar must neither dismiss the dialog nor drag-scroll it
            ImVec2 winPos = ImGui::GetWindowPos();
            ImVec2 winSize = ImGui::GetWindowSize();
            bool onScrollbar = (ImGui::GetScrollMaxY() > 0.0f) && (io.MousePos.x >= winPos.x + winSize.x - ImGui::GetStyle().ScrollbarSize);
            dismissArmed = !onScrollbar;
        }

        // Drag anywhere to scroll (touch screens); latched on the press so it keeps working
        // when the finger slides past the window edge
        if (dismissArmed && ImGui::IsMouseDragging(ImGuiMouseButton_Left, dragThreshold)) {
            ImGui::SetScrollY(ImGui::GetScrollY() - io.MouseDelta.y);
        }

        // Dismiss on Escape, or on a tap/click that didn't turn into a scroll drag
        bool tapped = dismissArmed && ImGui::IsMouseReleased(ImGuiMouseButton_Left) && (io.MouseDragMaxDistanceSqr[ImGuiMouseButton_Left] < dragThreshold * dragThreshold);
        if (tapped || PopupDialog::cancelKeyPressed()) {
            dismissArmed = false;
            ImGui::CloseCurrentPopup();
            open = false;
        }

        ImGui::EndPopup();
        ImGui::PopStyleColor();
#ifdef __ANDROID__
        ImGui::PopStyleVar(2);
#else
        ImGui::PopStyleVar();
#endif
        return open;
    }
}
