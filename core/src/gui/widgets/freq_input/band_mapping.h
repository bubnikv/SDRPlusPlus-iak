#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace freq_input {

    // The radio service whose band convention owns a stable band ID. Services
    // are deliberately independent of the band picker's coarse UI categories:
    // allocations belonging to different services may overlap in frequency.
    enum class BandService {
        Amateur,
        Broadcast,
        Aviation,
        Maritime,
        PersonalRadio,
        Ism,
        Satellite,
        Navigation,
        TimeStandard,
        Cellular,
        Rlan,
        Meteorological,
        LandMobile,
        Other
    };

    // What a legacy plan row represents. Only Band and Segment rows are
    // eligible for stable band IDs. Channels/bookmarks belong inside a band,
    // while generic L/S/C/X rows are service-independent spectrum ranges.
    enum class LegacyEntityKind {
        Band,
        Segment,
        Channel,
        Bookmark,
        SpectrumRange,
        ServiceEnvelope
    };

    // A service may contain several independently mapped band families. This
    // prevents, for example, a television row containing an FM probe from
    // receiving a sound-broadcast band ID.
    enum class BandFamily {
        Unknown,
        Amateur,
        SoundBroadcast,
        TelevisionBroadcast,
        WeatherBroadcast,
        AviationCommunication,
        AviationSurveillance,
        Maritime,
        PersonalRadio,
        IndustrialScientificMedical,
        Rlan,
        Satellite,
        Navigation,
        TimeStandard,
        CellularGsm,
        CellularLte,
        CellularOther,
        Meteorological,
        LandMobile,
        Spectrum
    };

    struct LegacyBandClassification {
        BandService service = BandService::Other;
        BandFamily family = BandFamily::Unknown;
        LegacyEntityKind entityKind = LegacyEntityKind::Band;
    };

    // The largest probe set currently required by a canonical band.
    // Probes identify legacy plan segments; they are not default tuning
    // frequencies and may intentionally sit on an inclusive segment boundary.
    constexpr std::size_t MAX_BAND_PROBES = 14;

    struct BandMapping {
        BandService service;
        BandFamily family;
        std::string_view name;
        std::string_view bandId;
        std::array<std::int64_t, MAX_BAND_PROBES> probesHz;
        std::size_t probeCount;
    };

    std::string_view bandServiceKey(BandService service);
    BandService bandServiceFromKey(std::string_view key);
    std::string_view bandFamilyKey(BandFamily family);
    BandFamily bandFamilyFromKey(std::string_view key);
    std::string_view legacyEntityKindKey(LegacyEntityKind kind);
    LegacyEntityKind legacyEntityKindFromKey(std::string_view key);

    // Static canonical mappings for one service, ordered by frequency.
    const BandMapping* bandMappings(BandService service, std::size_t& count);

    // Finds the one canonical band in `service` having a probe in the inclusive
    // legacy segment [startHz, endHz]. Returns null for invalid, unmatched, or
    // same-service ambiguous spans. It never searches across services.
    const BandMapping* findBandMapping(
        BandService service,
        double startHz,
        double endHz);

    // Family-qualified lookup used by legacy conversion. It never searches
    // another family belonging to the same service.
    const BandMapping* findBandMapping(
        BandService service,
        BandFamily family,
        double startHz,
        double endHz);

    // Normalize a legacy band-plan row into its entity kind, service, and
    // family. The old `type` strings are useful hints but contain known
    // misclassifications, so contextual name and span rules take precedence.
    LegacyBandClassification classifyLegacyBand(
        std::string_view type,
        std::string_view name,
        double startHz,
        double endHz);

    // Compatibility helper for callers interested only in the service.
    BandService classifyLegacyBandService(
        std::string_view type,
        std::string_view name);

    // Resolve an eligible legacy band/segment through its service and family.
    // Channels, bookmarks, generic spectrum ranges, and service envelopes
    // never manufacture a band ID.
    const BandMapping* findLegacyBandMapping(
        const LegacyBandClassification& classification,
        double startHz,
        double endHz);

}
