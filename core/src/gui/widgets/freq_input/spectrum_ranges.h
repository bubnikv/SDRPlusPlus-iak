#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace freq_input {

    // A service-independent, continuous RF navigation range. Unlike a radio
    // service band, these ranges never overlap and do not participate in band
    // stacking.
    struct SpectrumRange {
        std::string_view name;
        // Canonical, case-sensitive identity. Standard names and band letters
        // retain their official capitalization; callers must not case-fold it.
        std::string_view rangeId;
        std::int64_t startHz;
        std::int64_t endHz;

        constexpr bool containsFrequency(
            std::int64_t frequencyHz) const noexcept
        {
            return frequencyHz >= startHz && frequencyHz < endHz;
        }
    };

    // The portion of a canonical range that the active source can tune.
    // Clipping is runtime state and therefore never changes rangeId.
    struct AvailableSpectrumRange {
        const SpectrumRange* range = nullptr;
        std::int64_t startHz = 0;
        std::int64_t endHz = 0;
        bool partial = false;

        constexpr bool containsFrequency(double frequencyHz) const noexcept {
            return frequencyHz >= static_cast<double>(startHz) &&
                frequencyHz < static_cast<double>(endHz);
        }
    };

    // The default continuous ITU/IEEE analyzer-navigation table, ordered by
    // frequency. Ranges use [startHz, endHz) containment so exact boundaries
    // belong to only one entry.
    const SpectrumRange* spectrumRanges(std::size_t& count);
    const SpectrumRange* findSpectrumRange(std::string_view rangeId);
    const SpectrumRange* spectrumRangeAtFrequency(std::int64_t frequencyHz);

    // Intersect a canonical range with the source's display-domain tuning
    // limits. With limited=false the whole canonical range is returned.
    bool availableSpectrumRange(
        const SpectrumRange& range,
        bool limited,
        std::int64_t sourceMinHz,
        std::int64_t sourceMaxHz,
        AvailableSpectrumRange& available);

}
