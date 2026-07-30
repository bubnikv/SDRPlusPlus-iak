#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace freq_input {

    // Nomenclature owning a range's public name and stable ID. The selector
    // deliberately combines the globally useful ITU names at lower
    // frequencies with the more familiar IEEE microwave names.
    enum class SpectrumRangeScheme {
        Itu,
        Ieee521
    };

    // A service-independent, continuous RF navigation range. Unlike a radio
    // service band, these ranges never overlap and do not participate in band
    // stacking.
    struct SpectrumRange {
        SpectrumRangeScheme scheme;
        std::string_view name;
        std::string_view rangeId;
        std::int64_t startHz;
        std::int64_t endHz;
    };

    // The portion of a canonical range that the active source can tune.
    // Clipping is runtime state and therefore never changes rangeId.
    struct AvailableSpectrumRange {
        const SpectrumRange* range = nullptr;
        std::int64_t startHz = 0;
        std::int64_t endHz = 0;
        bool partial = false;
    };

    std::string_view spectrumRangeSchemeKey(SpectrumRangeScheme scheme);

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
