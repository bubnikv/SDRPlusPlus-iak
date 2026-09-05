#include <gui/dialogs/loading_screen.h>
#include <gui/main_window.h>
#include <imgui.h>
#include <gui/icons.h>
#include <gui/style.h>
#include <credits.h>
#include <gui/gui.h>
#include <backend.h>

namespace LoadingScreen {
    ImVec2 imageSize(128.0f, 128.0f);

    void init() {
        imageSize = style::dp(128.0f, 128.0f);
    }

    void show(std::string msg) {
        backend::beginFrame();
        imageSize = style::dp(128.0f, 128.0f);

        ImGui::Begin("Main", NULL, WINDOW_FLAGS);


        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, style::dp(20.0f, 20.0f));
        ImGui::OpenPopup("Credits");
        ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::BeginPopupModal("Credits", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground);

        ImGui::PushFont(style::hugeFont);
        ImGui::TextUnformatted("SDRIAK");
        ImGui::PopFont();
        ImGui::SameLine(0.0f, style::dp(40.0f));
        ImGui::Image(icons::LOGO, imageSize);

        ImVec2 origPos = ImGui::GetCursorPos();
        ImGui::SetCursorPosY(origPos.y + style::dp(50.0f));
        ImGui::Text("%s", msg.c_str());
        ImGui::SetCursorPos(origPos);

        ImGui::EndPopup();
        ImGui::PopStyleVar(1);
        ImGui::PopStyleColor(1);

        ImGui::End();

        backend::render(false);
    }
}
