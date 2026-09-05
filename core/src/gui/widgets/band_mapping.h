#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

// Legacy band identity bridge.
//
// This file does not define legal allocations or the frequency spans displayed
// by a band plan. It defines stable semantic band IDs and the service/family
// vocabulary used while SDRIAK still loads the country JSON files under
// root/res/bandplans. band_mapping.cpp identifies an eligible legacy Band or
// Segment when exactly one same-service/same-family canonical mapping has a
// probe inside that row.
//
// A probe is therefore an identity sample, not a center frequency, tuning
// default, channel, allocation edge, or claim that the entire surrounding
// range belongs to a service. Several probes are needed for fragmented and
// region-dependent legacy rows. The per-band source map and probe rationale
// live beside each mapping in band_mapping.cpp; the exhaustive legacy rows
// produced by those decisions are in doc/research/legacy-band-id-audit.md.
//
// This bridge protects band-stack memories during the temporary legacy-data
// release. The layered frequency catalog may later consume the same stable IDs,
// but its scoped Segments remain the source of frequency bounds.
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
        Other,
        Count
    };

    // A value-semantic set of radio services. Presentation layers may group
    // services differently without teaching domain code about group IDs or
    // labels.
    class BandServiceSet {
    public:
        constexpr BandServiceSet() noexcept = default;

        static constexpr BandServiceSet single(BandService service) noexcept {
            return BandServiceSet(bit(service));
        }

        static constexpr BandServiceSet all() noexcept {
            return BandServiceSet(
                (std::uint32_t{1} <<
                    static_cast<std::uint32_t>(BandService::Count)) - 1);
        }

        constexpr bool contains(BandService service) const noexcept {
            return (bits & bit(service)) != 0;
        }

        constexpr bool intersects(BandServiceSet other) const noexcept {
            return (bits & other.bits) != 0;
        }

        constexpr BandServiceSet with(BandService service) const noexcept {
            return BandServiceSet(bits | bit(service));
        }

        constexpr BandServiceSet without(BandService service) const noexcept {
            return BandServiceSet(bits & ~bit(service));
        }

        constexpr BandServiceSet merged(BandServiceSet other) const noexcept {
            return BandServiceSet(bits | other.bits);
        }

        constexpr bool operator==(BandServiceSet other) const noexcept {
            return bits == other.bits;
        }

        constexpr bool operator!=(BandServiceSet other) const noexcept {
            return !(*this == other);
        }

    private:
        explicit constexpr BandServiceSet(std::uint32_t value) noexcept
            : bits(value) {}

        static constexpr std::uint32_t bit(BandService service) noexcept {
            return std::uint32_t{1} <<
                static_cast<std::uint32_t>(service);
        }

        std::uint32_t bits = 0;
    };

    static_assert(
        static_cast<std::uint32_t>(BandService::Count) < 32,
        "BandServiceSet requires BandService to fit in a 32-bit mask");

    // What a legacy plan row represents. Only Band and Segment rows are
    // eligible for stable band IDs. Individual frequency assignments belong
    // inside a band, while generic L/S/C/X rows are service-independent
    // spectrum ranges.
    enum class LegacyEntityKind {
        Band,
        Segment,
        Channel,
        SpectrumRange
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

        bool isBandOrSegment() const noexcept {
            return entityKind == LegacyEntityKind::Band ||
                entityKind == LegacyEntityKind::Segment;
        }
    };

    struct BandMapping {
        BandService service;
        BandFamily family;
        // Canonical descriptive name and explicit band-selector presentation.
        // Selector text is metadata belonging to the stable identity; it must
        // never be inferred from a regional segment edge or parsed from name.
        std::string_view name;
        std::string_view selectorLabel;
        std::string_view selectorDetail;
        // Canonical, case-sensitive identity. Acronyms and SI symbols retain
        // their official capitalization; callers must not case-fold it.
        std::string_view bandId;
        // Probe storage is packed once per service. These fields select this
        // mapping's exact-size slice from BandMappingTable::probesHz.
        std::uint16_t probeOffset;
        std::uint8_t probeCount;
    };

    struct BandMappingTable {
        const BandMapping* mappings = nullptr;
        std::size_t mappingCount = 0;
        const std::int64_t* probesHz = nullptr;
        std::size_t probeCount = 0;
    };

    std::string_view bandServiceKey(BandService service);
    BandService bandServiceFromKey(std::string_view key);
    std::string_view bandFamilyKey(BandFamily family);
    std::string_view legacyEntityKindKey(LegacyEntityKind kind);

    // Static canonical mappings and their packed probe pool for one service,
    // ordered by frequency. Mapping probe offsets are relative to this table.
    BandMappingTable bandMappings(BandService service);

    // Exact, case-sensitive stable-ID lookup across the static registries.
    // Returned pointers remain valid for the application lifetime.
    const BandMapping* findBandMappingById(std::string_view bandId);

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

    // Resolve an eligible legacy band/segment through its service and family.
    // Individual frequency assignments and generic spectrum ranges never
    // manufacture a band ID.
    const BandMapping* findLegacyBandMapping(
        const LegacyBandClassification& classification,
        double startHz,
        double endHz);

}
