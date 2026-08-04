#include <backend_common.h>
#include <algorithm>
#include <cmath>
#include <core.h>
#include <gui/gui.h>
#include <gui/menus/theme.h>
#include <gui/style.h>
#include <imgui_impl_opengl3.h>
#include <utils/flog.h>
#include <utils/opengl_include_code.h>

namespace backend::common {
    static float effectiveScale(float detectedContentScale, float userScaleFactor) {
        return std::clamp(detectedContentScale * userScaleFactor, 1.0f, 4.0f);
    }

    float configUserScaleFactor() {
        return core::configManager.value("uiScaleFactor", 1.0f);
    }

    void initScaleState(ScaleState& state, const std::string& resDir, float detectedContentScale, float userScaleFactor) {
        state.resDir = resDir;
        state.detectedContentScale = detectedContentScale;
        state.userScaleFactor = userScaleFactor;
        state.pendingContentScale.store(0.0f);
        state.pendingUserScaleFactor = 0.0f;
    }

    float setScaleFromConfig(ScaleState& state, float detectedContentScale) {
        float oldScale = style::uiScale;
        state.detectedContentScale = detectedContentScale;
        state.userScaleFactor = configUserScaleFactor();
        style::setUIScale(effectiveScale(state.detectedContentScale, state.userScaleFactor));
        return oldScale;
    }

    void queueContentScale(ScaleState& state, float detectedContentScale) {
        state.pendingContentScale.store(detectedContentScale);
    }

    void setUserScaleFactor(ScaleState& state, float userScaleFactor) {
        // Defer the real apply until before NewFrame(), when the font atlas is not locked.
        state.pendingUserScaleFactor = userScaleFactor;
        core::configManager.set("uiScaleFactor", userScaleFactor);
    }

    bool applyScale(ScaleState& state, float detectedContentScale, float userScaleFactor, bool rebuildFonts, bool rescaleMainWindow) {
        float effective = effectiveScale(detectedContentScale, userScaleFactor);
        float oldScale = style::uiScale;
        if (std::fabs(effective - oldScale) < 0.0001f &&
            std::fabs(detectedContentScale - state.detectedContentScale) < 0.0001f &&
            std::fabs(userScaleFactor - state.userScaleFactor) < 0.0001f) {
            return false;
        }

        flog::info("Scale: {:.3f} -> {:.3f} (detected={:.2f}, factor={:.2f})", oldScale, effective, detectedContentScale, userScaleFactor);
        state.detectedContentScale = detectedContentScale;
        state.userScaleFactor = userScaleFactor;
        style::setUIScale(effective);

        if (!ImGui::GetCurrentContext()) { return true; }

        style::applyScaledStyle(thememenu::applyTheme);
        if (rebuildFonts && !state.resDir.empty()) {
            ImGui::GetIO().Fonts->Clear();
            style::loadFonts(state.resDir);
            ImGui_ImplOpenGL3_DestroyFontsTexture();
            ImGui_ImplOpenGL3_CreateFontsTexture();
        }
        if (rescaleMainWindow) {
            gui::mainWindow.onContentScaleChanged(oldScale);
        }
        return true;
    }

    bool applyPendingScaleChanges(ScaleState& state, float currentDetectedContentScale, bool checkCurrentDetected) {
        bool changed = false;
        float pendingDetected = state.pendingContentScale.exchange(0.0f);
        if (pendingDetected <= 0.0f && checkCurrentDetected &&
            std::fabs(currentDetectedContentScale - state.detectedContentScale) >= 0.0001f) {
            pendingDetected = currentDetectedContentScale;
        }
        if (pendingDetected > 0.0f) {
            changed |= applyScale(state, pendingDetected, state.userScaleFactor, true, true);
        }

        if (state.pendingUserScaleFactor > 0.0f) {
            float factor = state.pendingUserScaleFactor;
            state.pendingUserScaleFactor = 0.0f;
            changed |= applyScale(state, state.detectedContentScale, factor, true, true);
        }
        return changed;
    }

    void drawMainWindow(const ImVec2& size) {
        if (size.x <= 0.0f || size.y <= 0.0f) { return; }
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(size);
        gui::mainWindow.draw();
    }

    void renderOpenGLFrame(int width, int height) {
        ImGui::Render();
        glViewport(0, 0, width, height);
        glClearColor(gui::themeManager.clearColor.x, gui::themeManager.clearColor.y, gui::themeManager.clearColor.z, gui::themeManager.clearColor.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }
}
