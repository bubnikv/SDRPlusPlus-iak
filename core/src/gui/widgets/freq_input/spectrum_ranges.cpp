#include <gui/widgets/freq_input/spectrum_ranges.h>

#include <algorithm>
#include <array>

namespace freq_input {

    namespace {

        // A continuous human-facing RF partition:
        //
        //  * ITU-R V.431 decimal nomenclature through VHF.
        //  * IEEE Std 521-2026 radar-frequency nomenclature from UHF upward,
        //    where letter bands are substantially more useful to listeners
        //    than the decade-wide ITU UHF/SHF/EHF names.
        //
        // The scheme-qualified IDs prevent IEEE "UHF" (300 MHz-1 GHz) from
        // being confused with ITU UHF (300 MHz-3 GHz).
        static constexpr auto SPECTRUM_RANGES = std::array{
            SpectrumRange{
                SpectrumRangeScheme::Itu,
                "VLF",
                "spectrum:itu:vlf",
                3'000LL,
                30'000LL
            },
            SpectrumRange{
                SpectrumRangeScheme::Itu,
                "LF",
                "spectrum:itu:lf",
                30'000LL,
                300'000LL
            },
            SpectrumRange{
                SpectrumRangeScheme::Itu,
                "MF",
                "spectrum:itu:mf",
                300'000LL,
                3'000'000LL
            },
            SpectrumRange{
                SpectrumRangeScheme::Itu,
                "HF",
                "spectrum:itu:hf",
                3'000'000LL,
                30'000'000LL
            },
            SpectrumRange{
                SpectrumRangeScheme::Itu,
                "VHF",
                "spectrum:itu:vhf",
                30'000'000LL,
                300'000'000LL
            },
            SpectrumRange{
                SpectrumRangeScheme::Ieee521,
                "UHF",
                "spectrum:ieee521-2026:uhf",
                300'000'000LL,
                1'000'000'000LL
            },
            SpectrumRange{
                SpectrumRangeScheme::Ieee521,
                "L",
                "spectrum:ieee521-2026:l",
                1'000'000'000LL,
                2'000'000'000LL
            },
            SpectrumRange{
                SpectrumRangeScheme::Ieee521,
                "S",
                "spectrum:ieee521-2026:s",
                2'000'000'000LL,
                4'000'000'000LL
            },
            SpectrumRange{
                SpectrumRangeScheme::Ieee521,
                "C",
                "spectrum:ieee521-2026:c",
                4'000'000'000LL,
                8'000'000'000LL
            },
            SpectrumRange{
                SpectrumRangeScheme::Ieee521,
                "X",
                "spectrum:ieee521-2026:x",
                8'000'000'000LL,
                12'000'000'000LL
            },
            SpectrumRange{
                SpectrumRangeScheme::Ieee521,
                "Ku",
                "spectrum:ieee521-2026:ku",
                12'000'000'000LL,
                18'000'000'000LL
            },
            SpectrumRange{
                SpectrumRangeScheme::Ieee521,
                "K",
                "spectrum:ieee521-2026:k",
                18'000'000'000LL,
                27'000'000'000LL
            },
            SpectrumRange{
                SpectrumRangeScheme::Ieee521,
                "Ka",
                "spectrum:ieee521-2026:ka",
                27'000'000'000LL,
                40'000'000'000LL
            },
            SpectrumRange{
                SpectrumRangeScheme::Ieee521,
                "V",
                "spectrum:ieee521-2026:v",
                40'000'000'000LL,
                75'000'000'000LL
            },
            SpectrumRange{
                SpectrumRangeScheme::Ieee521,
                "W",
                "spectrum:ieee521-2026:w",
                75'000'000'000LL,
                110'000'000'000LL
            }
        };

        constexpr bool validSpectrumRanges() {
            for (std::size_t i = 0; i < SPECTRUM_RANGES.size(); i++) {
                const SpectrumRange& range = SPECTRUM_RANGES[i];
                if (range.name.empty() || range.rangeId.empty() ||
                    range.startHz >= range.endHz)
                {
                    return false;
                }
                if (i > 0 &&
                    SPECTRUM_RANGES[i - 1].endHz != range.startHz)
                {
                    return false;
                }
                for (std::size_t j = 0; j < i; j++) {
                    if (SPECTRUM_RANGES[j].rangeId == range.rangeId) {
                        return false;
                    }
                }
            }
            return true;
        }

        static_assert(
            validSpectrumRanges(),
            "Spectrum navigation ranges must be valid, continuous, "
            "non-overlapping, and uniquely identified");

    }

    std::string_view spectrumRangeSchemeKey(SpectrumRangeScheme scheme) {
        switch (scheme) {
            case SpectrumRangeScheme::Itu: return "itu";
            case SpectrumRangeScheme::Ieee521: return "ieee521-2026";
        }
        return "itu";
    }

    const SpectrumRange* spectrumRanges(std::size_t& count) {
        count = SPECTRUM_RANGES.size();
        return SPECTRUM_RANGES.data();
    }

    const SpectrumRange* findSpectrumRange(std::string_view rangeId) {
        for (const SpectrumRange& range : SPECTRUM_RANGES) {
            if (range.rangeId == rangeId) { return &range; }
        }
        return nullptr;
    }

    const SpectrumRange* spectrumRangeAtFrequency(
        std::int64_t frequencyHz)
    {
        for (const SpectrumRange& range : SPECTRUM_RANGES) {
            if (frequencyHz >= range.startHz && frequencyHz < range.endHz) {
                return &range;
            }
        }
        return nullptr;
    }

    bool availableSpectrumRange(
        const SpectrumRange& range,
        bool limited,
        std::int64_t sourceMinHz,
        std::int64_t sourceMaxHz,
        AvailableSpectrumRange& available)
    {
        std::int64_t lo = range.startHz;
        std::int64_t hi = range.endHz;
        if (limited) {
            if (sourceMinHz > sourceMaxHz) {
                std::swap(sourceMinHz, sourceMaxHz);
            }
            lo = std::max(lo, sourceMinHz);
            hi = std::min(hi, sourceMaxHz);
        }
        if (lo >= hi) {
            available = {};
            return false;
        }
        available.range = &range;
        available.startHz = lo;
        available.endHz = hi;
        available.partial =
            lo != range.startHz || hi != range.endHz;
        return true;
    }

}
