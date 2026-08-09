#include <gui/widgets/freq_input.h>
#include <gui/widgets/freq_input/spectrum_ranges.h>
#include <gui/widgets/freq_memory.h>
#include <gui/widgets/toggle_style.h>
#include <gui/style.h>
#include <config.h>
#include <core.h>

#include <algorithm>
#include <cinttypes>
#include <cstdio>
#include <string>
#include <vector>

namespace freq_input {

    namespace {

        std::string compactFrequency(std::int64_t frequencyHz) {
            char text[32];
            if (frequencyHz >= 1'000'000'000LL) {
                const double value = frequencyHz / 1e9;
                snprintf(
                    text,
                    sizeof(text),
                    value == static_cast<std::int64_t>(value)
                        ? "%.0f GHz" : "%.3g GHz",
                    value);
            }
            else if (frequencyHz >= 1'000'000LL) {
                const double value = frequencyHz / 1e6;
                snprintf(
                    text,
                    sizeof(text),
                    value == static_cast<std::int64_t>(value)
                        ? "%.0f MHz" : "%.3g MHz",
                    value);
            }
            else if (frequencyHz >= 1'000LL) {
                const double value = frequencyHz / 1e3;
                snprintf(
                    text,
                    sizeof(text),
                    value == static_cast<std::int64_t>(value)
                        ? "%.0f kHz" : "%.3g kHz",
                    value);
            }
            else {
                snprintf(text, sizeof(text), "%" PRId64 " Hz", frequencyHz);
            }
            return text;
        }

        std::string keyFrequency(std::int64_t frequencyHz) {
            char text[24];
            if (frequencyHz >= 1'000'000'000LL) {
                snprintf(text, sizeof(text), "%.3gG", frequencyHz / 1e9);
            }
            else if (frequencyHz >= 1'000'000LL) {
                snprintf(text, sizeof(text), "%.3gM", frequencyHz / 1e6);
            }
            else if (frequencyHz >= 1'000LL) {
                snprintf(text, sizeof(text), "%.3gk", frequencyHz / 1e3);
            }
            else {
                snprintf(text, sizeof(text), "%" PRId64, frequencyHz);
            }
            return text;
        }

        std::string compactRange(
            std::int64_t startHz,
            std::int64_t endHz)
        {
            return compactFrequency(startHz) + " - " +
                compactFrequency(endHz);
        }

        std::int64_t midpoint(const AvailableSpectrumRange& range) {
            return range.startHz + (range.endHz - range.startHz) / 2;
        }

        std::int64_t rememberedFrequency(
            ConfigManager::EditSection memory,
            const AvailableSpectrumRange& range)
        {
            double remembered = 0.0;
            if (!memory.tryGet(range.range->rangeId, remembered) ||
                !range.containsFrequency(remembered))
            {
                return midpoint(range);
            }
            return static_cast<std::int64_t>(remembered);
        }

    }

    void Spectrum::onOpen(const ConfigManager::ReadAccess& configAccess) {
        lastRangeId = freq_memory::spectrum(configAccess).value(
            freq_memory::ACTIVE_RANGE,
            "");
    }

    // Which grid owns the frequency memory: the lifecycle coordinator reads the
    // selector to decide where the current frequency belongs when written back.
    void Spectrum::onActivate(ConfigManager::EditAccess& configAccess) {
        freq_memory::activate(configAccess, freq_memory::SELECTOR_SPECTRUM);
    }

    Outcome Spectrum::draw(const Context& ctx, const Metrics& m) {
        Outcome out;
        const ImVec2 spacing = ImGui::GetStyle().ItemSpacing;

        std::size_t rangeCount = 0;
        const SpectrumRange* definitions =
            spectrumRanges(rangeCount);
        std::vector<AvailableSpectrumRange> ranges;
        ranges.reserve(rangeCount);
        bool hasPartialRange = false;
        for (std::size_t i = 0; i < rangeCount; i++) {
            AvailableSpectrumRange available;
            if (availableSpectrumRange(
                    definitions[i],
                    ctx.limited,
                    static_cast<std::int64_t>(ctx.rangeLo()),
                    static_cast<std::int64_t>(ctx.rangeHi()),
                    available))
            {
                hasPartialRange =
                    hasPartialRange || available.partial;
                ranges.push_back(available);
            }
        }

        const std::int64_t currentFrequency =
            static_cast<std::int64_t>(ctx.frequency);
        const SpectrumRange* active = nullptr;
        const SpectrumRange* persisted =
            findSpectrumRange(lastRangeId);
        if (persisted && persisted->containsFrequency(currentFrequency)) {
            active = persisted;
        }
        else {
            active = spectrumRangeAtFrequency(currentFrequency);
        }
        if (active &&
            lastRangeId != std::string(active->rangeId))
        {
            lastRangeId = std::string(active->rangeId);
            auto configAccess = core::configManager.edit();
            freq_memory::spectrum(configAccess).set(freq_memory::ACTIVE_RANGE, lastRangeId);
        }

        if (ranges.empty()) {
            ImGui::TextDisabled("No spectrum ranges in the tuning range");
        }
        else {
            // Columns follow the width, rows what is left of the page under
            // the heading and above the source-limited note.
            const int columns = m.spectrumCols;
            const float keyWidth =
                (m.gridWidth - (columns - 1) * spacing.x) / columns;
            const float keyHeight = m.spectrumKeyHeight;
            const int rowsNeeded =
                (static_cast<int>(ranges.size()) + columns - 1) / columns;
            const float lineHeight = ImGui::GetTextLineHeightWithSpacing();
            const int rowsFit = Metrics::rowsThatFit(
                m.pageHeight - lineHeight - (hasPartialRange ? lineHeight : 0.0f),
                keyHeight);
            const float gridHeight =
                Metrics::gridHeight(rowsNeeded, rowsFit, keyHeight);

            ImGui::BeginChild(
                "##sdrpp_spectrum_grid",
                ImVec2(m.totalWidth, gridHeight),
                false);
            const style::SelectedToggleColors selectedColors =
                style::selectedToggleColors();
            for (int i = 0; i < static_cast<int>(ranges.size()); i++) {
                const AvailableSpectrumRange& available = ranges[i];
                const SpectrumRange& range = *available.range;
                ImGui::SetCursorPos(ImVec2(
                    (i % columns) * (keyWidth + spacing.x),
                    (i / columns) * (keyHeight + spacing.y)));

                std::string label(range.name);
                if (available.partial) { label += "*"; }
                label += "\n";
                label += keyFrequency(available.startHz);
                label += "-";
                label += keyFrequency(available.endHz);
                label += "##sdrpp_spectrum_";
                label += range.rangeId;

                // The range holding the current frequency takes the shared
                // latched look, not ImGuiCol_ButtonActive: that colour is what a
                // button flashes while pressed, and several themes barely
                // separate it from ImGuiCol_Button.
                const bool isActive = (active == &range);
                if (isActive) { style::pushSelectedToggle(selectedColors); }
                const bool selected =
                    ImGui::Button(
                        label.c_str(),
                        ImVec2(keyWidth, keyHeight));
                if (isActive) {
                    style::drawSelectedToggleStroke(selectedColors);
                    style::popSelectedToggle();
                }

                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip(
                        "%s\n%s\nStable ID: %s%s",
                        range.name.data(),
                        compactRange(range.startHz, range.endHz).c_str(),
                        range.rangeId.data(),
                        available.partial
                            ? "\nPartially covered by the active source" : "");
                }

                if (!selected) { continue; }

                {
                    auto configAccess = core::configManager.edit();
                    ConfigManager::EditSection spectrum = freq_memory::spectrum(configAccess);
                    ConfigManager::EditSection memory =
                        spectrum.section(freq_memory::LAST_FREQUENCY);

                    // Remember the current frequency for the one non-overlapping
                    // range that owns it. This is a one-slot navigation memory,
                    // intentionally separate from the service-band register stack.
                    if (active)
                    {
                        memory.set(active->rangeId, currentFrequency);
                    }

                    out.frequency = static_cast<std::uint64_t>(
                        rememberedFrequency(memory, available));
                    spectrum.set(freq_memory::ACTIVE_RANGE, range.rangeId);
                }

                lastRangeId = std::string(range.rangeId);
                out.commit = true;
                out.close = true;
            }
            ImGui::EndChild();
        }

        if (hasPartialRange) {
            ImGui::TextDisabled("* Range limited by the source");
        }
        return out;
    }

}
