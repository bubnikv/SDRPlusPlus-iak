#include <gui/waterfall_autorange.h>
#include <gui/gui.h>   // gui::waterfall
#include <gui/icons.h> // icons::CONTRAST
#include <core.h>      // core::configManager
#include <algorithm>
#include <cmath>

void WaterfallAutoRange::init(float* min, float* max, bool stickyFromConfig) {
    fftMin = min;
    fftMax = max;
    stickyEnabled = stickyFromConfig;
    initialized = false;
}

void WaterfallAutoRange::oneShotFit() {
    float newMin, newMax;
    if (!gui::waterfall.getAutorangeValues(newMin, newMax)) { return; }
    *fftMin = newMin;
    *fftMax = std::max<float>(newMax, *fftMin + 10);
    core::configManager.acquire();
    core::configManager.conf["min"] = *fftMin;
    core::configManager.conf["max"] = *fftMax;
    core::configManager.release(true);
}

void WaterfallAutoRange::setSticky(bool on) {
    stickyEnabled = on;
    initialized = false; // snap to the first fit on (re)acquire
    core::configManager.acquire();
    core::configManager.conf["waterfallAutoRange"] = stickyEnabled;
    if (!stickyEnabled) {
        // Persist wherever the auto-scaler left the range so the manual
        // sliders resume from there.
        core::configManager.conf["min"] = *fftMin;
        core::configManager.conf["max"] = *fftMax;
    }
    core::configManager.release(true);
}

void WaterfallAutoRange::drawButton(float footprint, const ImVec4& tint) {
    // Glyph + padding at a fixed ratio so the button keeps its total footprint
    // (the surrounding Min/Max slider layout and its centering depend on it).
    float pad = footprint * (2.5f / 20.0f);
    float size = footprint - 2.0f * pad;
    // Fill the button when latched so it reads as "on".
    ImVec4 bg = stickyEnabled ? ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive) : ImVec4(0, 0, 0, 0);
    bool clicked = ImGui::ImageButton(icons::CONTRAST, ImVec2(size, size), ImVec2(0, 0), ImVec2(1, 1), (int)pad, bg, tint);
    // Capture item state before any other ImGui call (the tooltip) moves the
    // "last item" these queries refer to.
    bool active = ImGui::IsItemActive();
    bool hovered = ImGui::IsItemHovered();
    if (hovered) {
        ImGui::SetTooltip("Click: auto-fit range once\nHold: %s continuous auto-range", stickyEnabled ? "disable" : "enable");
    }

    // Hold-to-latch: the long-press fires while the button is still held (so
    // the release doesn't also trigger the one-shot). longPressed is consumed
    // before it's reset so a release that lands outside the button can't leave
    // the button dead.
    if (active) {
        btnHold += ImGui::GetIO().DeltaTime;
        if (!longPressed && btnHold >= 0.5f) {
            longPressed = true;
            setSticky(!stickyEnabled);
        }
    }
    if (clicked && !longPressed) {
        oneShotFit(); // short click
    }
    if (!active) {
        btnHold = 0.0f;
        longPressed = false;
    }
}

void WaterfallAutoRange::update() {
    if (!stickyEnabled) { return; }

    // Throttle to ~10 Hz (the deep-research 4-10 Hz recommendation): for users
    // who enabled fullWaterfallUpdate, each range change forces a full
    // framebuffer recolor, so we must not do it every frame.
    const float step = 0.1f;
    updateAccum += ImGui::GetIO().DeltaTime;
    if (updateAccum < step) { return; }
    float dt = updateAccum;
    updateAccum = 0.0f;

    float tMin, tMax;
    if (!gui::waterfall.getAutorangeValues(tMin, tMax)) { return; }

    if (!initialized) {
        // First fit after latching / reacquisition: snap, don't glide.
        *fftMin = tMin;
        *fftMax = tMax;
        initialized = true;
    }
    else {
        // Asymmetric smoothing: follow a rising floor / growing signal quickly
        // so the display can't wash out, but relax downward slowly to avoid
        // flicker and range pumping. A large jump (band / device / gain
        // change) bypasses the glide and reacquires immediately.
        auto ema = [dt](float cur, float tgt, float tauRise, float tauFall) {
            if (std::abs(tgt - cur) > 25.0f) { return tgt; }
            float tau = (tgt > cur) ? tauRise : tauFall;
            float alpha = 1.0f - expf(-dt / tau);
            return cur + alpha * (tgt - cur);
        };
        *fftMin = ema(*fftMin, tMin, 0.6f, 3.0f);
        *fftMax = ema(*fftMax, tMax, 1.0f, 8.0f);
    }
    if (*fftMax < *fftMin + 10.0f) { *fftMax = *fftMin + 10.0f; }
}
