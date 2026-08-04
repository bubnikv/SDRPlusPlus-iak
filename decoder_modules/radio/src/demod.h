#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <string>
#include <type_traits>
#include <dsp/stream.h>
#include <dsp/types.h>
#include <gui/widgets/waterfall.h>
#include <gui/style.h>
#include <config.h>
#include <radio_interface.h>
#include <utils/event.h>

enum DeemphasisMode {
    DEEMP_MODE_22US,
    DEEMP_MODE_50US,
    DEEMP_MODE_75US,
    DEEMP_MODE_NONE,
    _DEEMP_MODE_COUNT
};

enum IFNRPreset {
    IFNR_PRESET_NOAA_APT,
    IFNR_PRESET_VOICE,
    IFNR_PRESET_NARROW_BAND,
    IFNR_PRESET_BROADCAST
};

enum SquelchMode {
    SQUELCH_MODE_OFF,
    SQUELCH_MODE_POWER,
    SQUELCH_MODE_SNR,
    SQUELCH_MODE_CTCSS_MUTE,
    SQUELCH_MODE_CTCSS_DECODE,
    SQUELCH_MODE_DCS_MUTE,
    SQUELCH_MODE_DCS_DECODE,
};

namespace demod {
    template <std::size_t PresetCount>
    static inline void drawHzPresetPopup(float currentValue, float minValue, float maxValue, const std::array<int, PresetCount>& presets, float* selectedValue) {
        ImGui::Text("Current: %.0f Hz", currentValue);
        ImGui::Separator();
        for (int presetValue : presets) {
            float preset = (float)presetValue;
            if (preset < minValue || preset > maxValue) { continue; }
            std::string label = std::to_string(presetValue) + " Hz";
            bool selected = (std::round(currentValue) == preset);
            if (ImGui::Selectable(label.c_str(), selected)) {
                *selectedValue = preset;
            }
        }
    }

    // Numeric input (int or float) with a "current" readout, -/+ repeat-step buttons and a
    // dropdown of Hz presets. The layout, stepping and popup are shared; only the input widget
    // and the popup-result conversion are type-specific (handled at the edges via if constexpr).
    template <typename T, std::size_t PresetCount>
    static inline bool showHzPresetInput(const char* id, T& value, T minValue, T maxValue, const std::array<int, PresetCount>& presets, T step, T stepFast) {
        static_assert(std::is_same_v<T, int> || std::is_same_v<T, float>, "showHzPresetInput supports int and float");
        ImGui::PushID(id);

        bool changed = false;
        ImGuiStyle& imguiStyle = ImGui::GetStyle();
        float buttonWidth = ImGui::GetFrameHeight();
        float spacing = imguiStyle.ItemInnerSpacing.x;
        float availableWidth = ImGui::GetContentRegionAvail().x;
        float inputWidth = std::max(1.0f, availableWidth - (3.0f * buttonWidth) - (3.0f * spacing));
        ImVec2 controlMin = ImGui::GetCursorScreenPos();
        float controlWidth = inputWidth + (3.0f * buttonWidth) + (3.0f * spacing);

        ImGui::SetNextItemWidth(inputWidth);
        T editedValue = value;
        bool edited;
        if constexpr (std::is_same_v<T, int>) {
            edited = ImGui::InputInt("##value", &editedValue, 0, 0);
        }
        else {
            edited = ImGui::InputFloat("##value", &editedValue, 0.0f, 0.0f, "%.0f");
        }
        if (edited) {
            editedValue = std::clamp<T>(editedValue, minValue, maxValue);
            if (editedValue != value) {
                value = editedValue;
                changed = true;
            }
        }

        T stepAmount = ImGui::GetIO().KeyCtrl ? stepFast : step;
        ImGui::PushButtonRepeat(true);
        ImGui::SameLine(0.0f, spacing);
        if (ImGui::Button("-##step_down", ImVec2(buttonWidth, 0.0f))) {
            T steppedValue = std::clamp<T>(value - stepAmount, minValue, maxValue);
            if (steppedValue != value) {
                value = steppedValue;
                changed = true;
            }
        }
        ImGui::SameLine(0.0f, spacing);
        if (ImGui::Button("+##step_up", ImVec2(buttonWidth, 0.0f))) {
            T steppedValue = std::clamp<T>(value + stepAmount, minValue, maxValue);
            if (steppedValue != value) {
                value = steppedValue;
                changed = true;
            }
        }
        ImGui::PopButtonRepeat();
        ImGui::SameLine(0.0f, spacing);
        if (ImGui::ArrowButton("##presets", ImGuiDir_Down)) {
            ImGui::OpenPopup("presets");
        }

        ImVec2 controlMax = ImGui::GetItemRectMax();
        ImGui::SetNextWindowPos(ImVec2(controlMin.x, controlMax.y), ImGuiCond_Appearing);
        ImGui::SetNextWindowSize(ImVec2(controlWidth, 0.0f), ImGuiCond_Appearing);
        if (ImGui::BeginPopup("presets")) {
            float selectedValue = (float)value;
            drawHzPresetPopup((float)value, (float)minValue, (float)maxValue, presets, &selectedValue);
            if constexpr (std::is_same_v<T, int>) {
                int newValue = std::clamp<int>((int)std::round(selectedValue), minValue, maxValue);
                if (newValue != value) {
                    value = newValue;
                    changed = true;
                }
            }
            else {
                // Presets are exact integers within range; assign directly (no round) so merely
                // opening the popup never perturbs a non-integer value.
                if (selectedValue != value) {
                    value = selectedValue;
                    changed = true;
                }
            }
            ImGui::EndPopup();
        }

        ImGui::PopID();
        return changed;
    }

    class Demodulator {
    public:
        virtual ~Demodulator() {}
        virtual void init(std::string name, ConfigManager* config, dsp::stream<dsp::complex_t>* input, double bandwidth, double audioSR) = 0;
        virtual void start() = 0;
        virtual void stop() = 0;
        virtual void showMenu() = 0;
        virtual void setBandwidth(double bandwidth) = 0;
        virtual void setInput(dsp::stream<dsp::complex_t>* input) = 0;
        virtual void AFSampRateChanged(double newSR) = 0;
        // Canonical mode name, from RADIO_IFACE_MODE_NAMES. Doubles as this
        // demodulator's key into radio_config.json, so it must stay the same
        // string the rest of the app shows.
        virtual const char* getName() = 0;
        virtual double getIFSampleRate() = 0;
        virtual double getAFSampleRate() = 0;
        virtual double getDefaultBandwidth() = 0;
        virtual double getMinBandwidth() = 0;
        virtual double getMaxBandwidth() = 0;
        virtual bool getBandwidthLocked() = 0;
        virtual double getDefaultSnapInterval() = 0;
        virtual int getVFOReference() = 0;
        virtual bool getDeempAllowed() = 0;
        virtual bool getPostProcEnabled() = 0;
        virtual int getDefaultDeemphasisMode() = 0;
        virtual bool getFMIFNRAllowed() = 0;
        virtual bool getNBAllowed() = 0;
        virtual bool getHighPassAllowed() = 0;
        virtual bool getSquelchAllowed() = 0;
        virtual dsp::stream<dsp::stereo_t>* getOutput() = 0;

    protected:
        friend struct AGCControls;

        // The demodulator's own config subtree, copied out under one transaction.
        // The loadConf() readers below then work on a plain document, so the
        // config lock is held for exactly one call rather than for the whole of
        // a constructor that also builds the DSP chain. Returns an empty object
        // for a demodulator the config has never seen.
        nlohmann::json loadSection() {
            auto txn = _config->transaction();
            return txn.section(name).value(getName(), nlohmann::json::object());
        }

        // Load a value from the demodulator's config section if present
        template <class T>
        static void loadConf(const nlohmann::json& j, const char* key, T& out) {
            if (j.contains(key)) { out = j[key]; }
        }

        // Save a value into the demodulator's config section
        template <class T>
        void saveConf(const char* key, const T& value) {
            auto txn = _config->transaction();
            txn.section(name, getName()).set(key, value);
        }

        ConfigManager* _config = NULL;
        std::string name;
    };

    // AGC state shared by the SSB and CW family demodulators: on/off switch, manual gain
    // and attack/decay, with the matching config load and menu UI. In auto mode the gain
    // slider shows the live AGC gain, in manual mode it sets a fixed gain.
    struct AGCControls {
        bool enabled = true;
        float gain = 0.0f;   // dB
        float attack = 50.0f;
        float decay = 5.0f;

        void load(const nlohmann::json& j) {
            Demodulator::loadConf(j, "agcEnabled", enabled);
            Demodulator::loadConf(j, "agcGain", gain);
            Demodulator::loadConf(j, "agcAttack", attack);
            Demodulator::loadConf(j, "agcDecay", decay);
        }

        // Apply the enabled state and manual gain to a freshly init()ed demodulator
        // (attack and decay are passed to the demodulator's init() directly)
        template <class TDemod>
        void apply(TDemod& demod) {
            demod.setAGCEnabled(enabled);
            demod.setAGCGain(powf(10.0f, gain / 20.0f));
        }

        template <class TDemod>
        void showMenu(TDemod& demod, Demodulator* owner, const char* idPrefix, double ifSamplerate, float menuWidth) {
            std::string id = std::string("##_radio_") + idPrefix;
            if (ImGui::Checkbox(("AGC" + id + "_agc_ena_" + owner->name).c_str(), &enabled)) {
                demod.setAGCEnabled(enabled);
                owner->saveConf("agcEnabled", enabled);
                if (!enabled) {
                    // Keep the last AGC gain as the manual gain
                    owner->saveConf("agcGain", gain);
                }
            }
            if (enabled) {
                gain = std::clamp<float>(20.0f * log10f(demod.getAGCGain()), -10.0f, 90.0f);
                ImGui::BeginDisabled();
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
            if (ImGui::SliderFloat((id + "_gain_" + owner->name).c_str(), &gain, -10.0f, 90.0f, "%.0f dB")) {
                demod.setAGCGain(powf(10.0f, gain / 20.0f));
                owner->saveConf("agcGain", gain);
            }
            if (enabled) { ImGui::EndDisabled(); }
            else { ImGui::BeginDisabled(); }
            ImGui::LeftLabel("AGC Attack");
            ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
            if (ImGui::SliderFloat((id + "_agc_attack_" + owner->name).c_str(), &attack, 1.0f, 200.0f)) {
                demod.setAGCAttack(attack / ifSamplerate);
                owner->saveConf("agcAttack", attack);
            }
            ImGui::LeftLabel("AGC Decay");
            ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
            if (ImGui::SliderFloat((id + "_agc_decay_" + owner->name).c_str(), &decay, 1.0f, 20.0f)) {
                demod.setAGCDecay(decay / ifSamplerate);
                owner->saveConf("agcDecay", decay);
            }
            if (!enabled) { ImGui::EndDisabled(); }
        }
    };
}

#include "demodulators/wfm.h"
#include "demodulators/nfm.h"
#include "demodulators/am.h"
#include "demodulators/usb.h"
#include "demodulators/lsb.h"
#include "demodulators/dsb.h"
#include "demodulators/cw.h"
#include "demodulators/cwr.h"
#include "demodulators/raw.h"
