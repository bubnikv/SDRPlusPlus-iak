#ifdef __ANDROID__
#include <gui/menus/android.h>
#include <imgui.h>
#include <gui/style.h>
#include <core.h>
#include <gui/gui.h>
#include <android_backend.h>
#include <algorithm>

namespace androidmenu {

    // Must match SleepTimerManager.Mode ordinals: DISABLED=0, KEEP_ALIVE=1, DIM_SCREEN=2, DIM_AND_BLANK=3
    static const char* SLEEP_MODE_ITEMS = "Disabled\0Keep Alive\0Dim Screen\0Dim and Blank\0";
    static const int   SLEEP_MODE_COUNT = 4;

    bool restartOnResume = true;  // restart SDR when app returns to foreground
    int sleepMode     = 3;   // default: Dim and Blank
    int screenDimMin  = 3;   // total minutes from start until screen dims
    int screenDarkMin = 8;   // total minutes from start until screen goes dark (> screenDimMin)

    static void applyConfig() {
        backend::setSleepTimerConfig(sleepMode, screenDimMin * 60, screenDarkMin * 60);
    }

    void init() {
        {
            auto configAccess = core::configManager.read();
            restartOnResume = configAccess.value("restartOnResume", true);
            sleepMode     = configAccess.value("sleepMode",    3);
            screenDimMin  = configAccess.value("sleepDimMin",  3);
            screenDarkMin = configAccess.value("sleepDarkMin", 8);
        }

        // Clamp / enforce invariants
        sleepMode     = std::clamp(sleepMode, 0, SLEEP_MODE_COUNT - 1);
        screenDimMin  = std::max(1, screenDimMin);
        screenDarkMin = std::max(screenDimMin + 1, screenDarkMin);

        backend::setRestartOnResume(restartOnResume);
        applyConfig();
    }

    void draw(void* ctx) {
        // ── Background behaviour ──────────────────────────────────────────────────
        if (ImGui::Checkbox("Restart Radio on Resume##android_restart_resume", &restartOnResume)) {
            backend::setRestartOnResume(restartOnResume);
            core::configManager.edit().set("restartOnResume", restartOnResume);
        }

        // ── Keep-alive mode ───────────────────────────────────────────────────────
        ImGui::LeftLabelFill("Keep Alive");
        if (ImGui::Combo("##android_sleep_mode", &sleepMode, SLEEP_MODE_ITEMS)) {
            sleepMode = std::clamp(sleepMode, 0, SLEEP_MODE_COUNT - 1);
            applyConfig();
            core::configManager.edit().set("sleepMode", sleepMode);
        }

        // ── Dim threshold (DIM_SCREEN=2 and DIM_AND_BLANK=3) ─────────────────────
        if (sleepMode >= 2) {
            bool changed = false;

            ImGui::LeftLabelFill("Dim screen after (min)");
            if (ImGui::InputInt("##android_dim_min", &screenDimMin, 1, 5)) {
                screenDimMin  = std::max(1, screenDimMin);
                screenDarkMin = std::max(screenDimMin + 1, screenDarkMin);
                changed = true;
            }

            // ── Blank threshold (DIM_AND_BLANK=3 only) ───────────────────────────
            if (sleepMode >= 3) {
                ImGui::LeftLabelFill("Screen off after (min)");
                if (ImGui::InputInt("##android_dark_min", &screenDarkMin, 1, 5)) {
                    screenDarkMin = std::max(screenDimMin + 1, screenDarkMin);
                    changed = true;
                }
            }

            if (changed) {
                applyConfig();
                auto configAccess = core::configManager.edit();
                configAccess.set("sleepDimMin",  screenDimMin);
                configAccess.set("sleepDarkMin", screenDarkMin);
            }
        }
    }

}

#endif
