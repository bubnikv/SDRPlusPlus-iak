#include <gui/widgets/freq_memory.h>
#include <gui/widgets/band_stack.h>
#include <gui/widgets/freq_input/spectrum_ranges.h>
#include <gui/gui.h>
#include <core.h>
#include <cmath>
#include <cstdint>
#include <string>

namespace freq_memory {

    void activate(ConfigManager::EditAccess& configAccess, std::string_view selector) {
        root(configAccess).set(SELECTOR, std::string(selector));
    }

    void commitCurrent() {
        std::string selector;
        {
            auto configAccess = core::configManager.read();
            selector = root(configAccess).value(
                SELECTOR,
                SELECTOR_BAND);
        }

        if (selector == SELECTOR_BAND) {
            gui::bandStack.commitCurrentBand();
            return;
        }
        if (selector != SELECTOR_SPECTRUM) { return; }

        const double currentFrequency = (double)gui::freqSelect.frequency;
        const freq_input::SpectrumRange* range =
            freq_input::spectrumRangeAtFrequency(
                (std::int64_t)std::llround(currentFrequency));
        if (!range) { return; }

        auto configAccess = core::configManager.edit();
        ConfigManager::EditSection spectrumMemory = spectrum(configAccess);
        spectrumMemory.section(LAST_FREQUENCY)
            .set(range->rangeId, currentFrequency);
        spectrumMemory.set(ACTIVE_RANGE, range->rangeId);
    }

}
