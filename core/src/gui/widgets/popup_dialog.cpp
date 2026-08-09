#include "popup_dialog.h"
#include <gui/style.h>

void PopupDialog::request() {
    open = true;
    armRequested = true;
}

bool PopupDialog::begin(const char* id, ImGuiWindowFlags flags) {
    if (!open) { return false; }
    if (armRequested) {
        ImGui::OpenPopup(id);
        armRequested = false;
    }
    if (!ImGui::BeginPopup(id, flags | ImGuiWindowFlags_NoMove)) {
        // Closed at the ImGui level (Android back gesture, click outside).
        open = false;
        return false;
    }
    // Sampled before any widget runs: a field being edited when the frame
    // starts still holds the active id here, even if this frame's Escape
    // press deactivates it during the field's own widget call.
    fieldWasActive = ImGui::IsAnyItemActive();
    focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows | ImGuiFocusedFlags_NoPopupHierarchy);
    return true;
}

void PopupDialog::end() {
    ImGui::EndPopup();
}

bool PopupDialog::applyButton(const char* label, bool disabled, bool suppressEnter) {
    bool requested = !disabled && applyRequested(suppressEnter);
    if (disabled) { style::beginDisabled(); }
    bool clicked = ImGui::Button(label);
    if (disabled) { style::endDisabled(); }
    return clicked || requested;
}

bool PopupDialog::cancelButton(const char* label) {
    return ImGui::Button(label) || cancelRequested();
}

bool PopupDialog::applyRequested(bool suppressEnter) const {
    return focused && !suppressEnter && confirmKeyPressed();
}

bool PopupDialog::cancelRequested() const {
    return focused && !fieldWasActive && cancelKeyPressed();
}

void PopupDialog::close() {
    open = false;
    ImGui::CloseCurrentPopup();
}

bool PopupDialog::confirmKeyPressed() {
    return ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false);
}

bool PopupDialog::cancelKeyPressed() {
    return ImGui::IsKeyPressed(ImGuiKey_Escape, false);
}
