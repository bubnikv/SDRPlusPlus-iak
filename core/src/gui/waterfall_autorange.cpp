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
    normalizeRange();
}

void WaterfallAutoRange::normalizeRange() {
    // Bring legacy / out-of-bounds config into the Ref+Range model on load:
    // Ref (= fftMin) in [-160,-30], Range (= fftMax-fftMin) in [30,160].
    // In-memory only; config is rewritten on the next slider change or when
    // sticky is toggled off.
    float ref = std::clamp(*fftMin, -160.0f, -30.0f);
    float range = std::clamp(*fftMax - *fftMin, 30.0f, 160.0f);
    *fftMin = ref;
    *fftMax = ref + range;
}

void WaterfallAutoRange::applyRef(float ref) {
    // Ref is the floor / bottom of the window. Slide the window to it while
    // preserving the user's Range (contrast). Ref stays within the slider's
    // bounds so the greyed Ref slider shows a valid position while sticky.
    float range = *fftMax - *fftMin;
    ref = std::clamp(ref, -160.0f, -30.0f);
    *fftMin = ref;
    *fftMax = ref + range;
}

void WaterfallAutoRange::oneShotFit() {
    // Ref only: re-level to the measured noise floor, keep the user's Range.
    // Pool the last 5 FFT lines so a single click can't land on a transient
    // whole-band burst (sticky uses 1 line -- its EMA smooths over time).
    float ref;
    bool ok = gui::waterfall.getAutorangeRef(ref, 5);
    // Release the 5x pool buffer (grown even on a failed fit) so the 1-line
    // sticky path doesn't hold 5x the memory.
    gui::waterfall.freeAutorangeScratch();
    if (!ok) { return; }
    applyRef(ref);
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
    // (the surrounding Range/Ref slider layout and its centering depend on it).
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

    float refTarget;
    if (!gui::waterfall.getAutorangeRef(refTarget)) { return; }

    if (!initialized) {
        // First fit after latching / reacquisition: snap, don't glide.
        applyRef(refTarget);
        initialized = true;
    }
    else {
        // Asymmetric smoothing on Ref (the noise floor): follow a rising floor
        // quickly so the display can't wash out, but relax downward slowly to
        // avoid flicker. A large jump (band / device / gain change) bypasses
        // the glide and reacquires immediately. applyRef preserves Range.
        float cur = *fftMin;
        float newRef;
        if (std::abs(refTarget - cur) > 25.0f) {
            newRef = refTarget;
        }
        else {
            float tau = (refTarget > cur) ? 0.6f : 3.0f;
            float alpha = 1.0f - expf(-dt / tau);
            newRef = cur + alpha * (refTarget - cur);
        }
        applyRef(newRef);
    }
}
