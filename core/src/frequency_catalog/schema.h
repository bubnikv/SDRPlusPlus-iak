#pragma once

#include <json.hpp>
#include <radio_interface.h>

#include <cstdint>
#include <functional>
#include <initializer_list>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace frequency_catalog {

    using nlohmann::json;

    // Version of the persisted catalog document defined in this file. Provider
    // cache manifests have their own version because their refresh metadata and
    // source-specific payloads evolve independently.
    inline constexpr int CATALOG_SCHEMA_VERSION = 1;
    // Android's packaged CBOR uses a compact-key wire representation which is
    // expanded to the canonical JSON schema before migration and validation.
    inline constexpr int CATALOG_CBOR_WIRE_SCHEMA_VERSION = 1;

    struct BandIdTag {};
    struct PlanIdTag {};
    struct SegmentIdTag {};
    struct BookmarkIdTag {};
    struct ProviderRecordIdTag {};

    // IDs are deliberately distinct C++ types. A Segment id must never become
    // a band-stack persistence key merely because both happen to be strings.
    template <typename Tag>
    class Id {
    public:
        Id() = default;
        explicit Id(std::string value) : value_(std::move(value)) {}

        const std::string& str() const { return value_; }
        bool empty() const { return value_.empty(); }

        friend bool operator==(const Id& a, const Id& b) { return a.value_ == b.value_; }
        friend bool operator!=(const Id& a, const Id& b) { return !(a == b); }
        friend bool operator<(const Id& a, const Id& b) { return a.value_ < b.value_; }

    private:
        std::string value_;
    };

    using BandId = Id<BandIdTag>;
    using PlanId = Id<PlanIdTag>;
    using SegmentId = Id<SegmentIdTag>;
    using BookmarkId = Id<BookmarkIdTag>;
    using ProviderRecordId = Id<ProviderRecordIdTag>;

    template <typename Tag>
    void to_json(json& j, const Id<Tag>& id) {
        j = id.str();
    }

    // Throws nlohmann::json::type_error for a non-string and
    // std::invalid_argument for an invalid stable id.
    template <typename Tag>
    void from_json(const json& j, Id<Tag>& id);

    enum class CatalogLayer {
        System,
        User,
        Dynamic
    };

    struct FrequencyRange {
        double minHz = 0.0;
        double maxHz = 0.0;

        bool contains(double frequency) const {
            return frequency >= minHz && frequency <= maxHz;
        }
    };

    struct GeoPoint {
        double latitude = 0.0;
        double longitude = 0.0;
    };

    // Provenance is retained when a dynamic result is copied to a user
    // bookmark. recordId identifies the source record; it is not the new
    // bookmark's identity.
    struct SourceRef {
        std::string provider;
        ProviderRecordId recordId;
        std::string upstreamId;
        std::string url;
    };

    enum class SegmentKind {
        Allocation,
        OperatingPlan,
        Usage,
        Application,
        ChannelPlan
    };

    enum class AllocationStatus {
        Unspecified,
        Primary,
        Secondary,
        Advisory
    };

    // Geographic applicability of a plan. An empty field means that the
    // source did not constrain that dimension; it never means that its
    // Segments own those frequencies globally.
    struct PlanScope {
        uint8_t ituRegionMask = 0; // bit 0 = Region 1, bit 1 = Region 2, bit 2 = Region 3
        std::vector<std::string> countryCodes;
        std::vector<std::string> subdivisions;
    };

    struct BandPlan {
        PlanId planId;
        std::string name;
        PlanScope scope;
        std::string source;
        std::string revision;
    };

    // Stable semantic identity only. Frequency limits and tuning defaults
    // belong to plan-scoped Segments because allocations overlap and vary by
    // ITU region and national administration.
    struct Band {
        BandId bandId;
        std::string name;
        std::string service;
    };

    // One contiguous statement made by a particular plan. Overlap is legal,
    // including between different services and between nested usage segments.
    struct Segment {
        SegmentId segmentId;
        PlanId planId;
        std::optional<BandId> bandId;
        std::string name;
        std::string service;
        SegmentKind kind = SegmentKind::Allocation;
        AllocationStatus status = AllocationStatus::Unspecified;
        FrequencyRange range;
        double defaultFrequency = 0.0;
        int defaultMode = -1;
        double channelSpacing = 0.0;
        std::optional<SourceRef> sourceRef;
    };

    // UTC recurring schedule shared by persisted bookmarks and EIBI records.
    // Bit 0 is Sunday, matching frequency_manager's current days[0] convention.
    // An end before start represents an interval crossing midnight. The end
    // may be 1440 as an exclusive end-of-day sentinel (EiBi uses 2400).
    struct WeeklySchedule {
        int startMinuteUtc = 0;
        int endMinuteUtc = 0;
        uint8_t dayMask = 0x7F;
        int validFromYmd = 0;   // YYYYMMDD, 0 = unspecified
        int validUntilYmd = 0;  // YYYYMMDD, 0 = unspecified
        std::string daysText;
    };

    // Persisted system or user tuning target. Dynamic provider records are
    // projected into this shape only for display/tuning; they remain stored as
    // their richer provider-specific records below.
    struct Bookmark {
        BookmarkId bookmarkId;
        CatalogLayer layer = CatalogLayer::User;
        std::optional<BandId> bandId;
        // Empty scope applies everywhere. System bookmarks imported from
        // OpenWebRX+ use ITU-region or country scope without pretending that
        // the frequency belongs exclusively to one plan.
        PlanScope scope;
        std::string name;
        double frequency = 0.0;
        double bandwidth = 0.0;
        int mode = -1;
        // The source may name a decoder that SDR++ cannot tune directly.
        // mode is the supported SDR++ fallback; these fields preserve the
        // original intent for future decoder-aware projections.
        std::string sourceMode;
        std::string underlyingMode;
        bool scannable = true;
        std::optional<WeeklySchedule> schedule;
        std::string notes;
        std::string geoInfo;
        std::optional<SourceRef> sourceRef;
    };

    struct EibiScheduleRecord {
        SourceRef sourceRef;
        std::optional<BandId> bandId;
        std::string station;
        std::string countryCode;
        std::string language;
        std::string target;
        // Raw CSV transmitter-site code and OpenWebRX-compatible normalized
        // lookup key. For example, "no" from Norway becomes "NOR-no", while
        // "/BLR-mo" becomes "BLR-mo".
        std::string transmitterSiteCode;
        std::string transmitterSite;
        std::string remarks;
        int persistenceCode = 0;
        // Retained verbatim because bracketed stop values are "last heard",
        // not validity dates, and persistence codes other than 6 alter how
        // these fields should be interpreted.
        std::string startDateText;
        std::string stopDateText;
        double carrierFrequency = 0.0;
        double tuningFrequency = 0.0;
        int mode = RADIO_IFACE_MODE_AM;
        WeeklySchedule schedule;
        std::optional<GeoPoint> transmitterLocation;
    };

    struct RepeaterRecord {
        SourceRef sourceRef;
        std::optional<BandId> bandId;
        std::string callsign;
        std::string name;
        double outputFrequency = 0.0;
        double inputFrequency = 0.0;
        // Provider capabilities, for example "FM", "DMR", "D-STAR", "YSF",
        // "NXDN", "P25" or "M17". These are not RADIO_IFACE_MODE_*: SDR++ may
        // not have a demodulator for every repeater protocol.
        std::vector<std::string> modes;
        // Optional SDR++ demodulator used when tuning this record. Provider
        // adapters choose it without discarding the capabilities above.
        int tuningMode = -1;
        std::string uplinkTone;
        std::string downlinkTone;
        std::string status;
        std::string lastUpdated;
        std::string notes;
        GeoPoint location;
    };

    // Static catalog document. Dynamic data uses provider snapshots and is not
    // mixed into this persisted system/user document.
    struct CatalogDocument {
        int schemaVersion = CATALOG_SCHEMA_VERSION;
        std::vector<BandPlan> plans;
        std::vector<Band> bands;
        std::vector<Segment> segments;
        std::vector<Bookmark> bookmarks;
    };

    bool isValidStableId(std::string_view value);
    bool isValidProviderName(std::string_view value);

    // Stable, cross-platform, non-cryptographic 128-bit fingerprint. Parts are
    // length-delimited, so {"ab", "c"} cannot collide structurally with
    // {"a", "bc"}. Provider record identity must be derived only from
    // normalized semantic fields, never from a row number or download time.
    ProviderRecordId makeProviderRecordId(
        std::string_view provider,
        std::initializer_list<std::string_view> normalizedParts);

    // Validates identity, references, ranges, modes and schedules. An empty
    // result means the document is valid.
    std::vector<std::string> validate(const CatalogDocument& document);
    std::vector<std::string> validate(const EibiScheduleRecord& record);
    std::vector<std::string> validate(const RepeaterRecord& record);

    // Applies migrations in place until CATALOG_SCHEMA_VERSION. Version 1 is
    // the first public schema, so unversioned input is intentionally rejected:
    // legacy band plans/bookmark configs require explicit import adapters that
    // can assign and persist stable IDs without guessing.
    bool migrateCatalogDocument(json& document, std::string& error);

    CatalogDocument catalogDocumentFromJson(json document);
    CatalogDocument catalogDocumentFromCbor(const std::vector<uint8_t>& data);
    json catalogDocumentToJson(const CatalogDocument& document);

    void to_json(json& j, CatalogLayer layer);
    void from_json(const json& j, CatalogLayer& layer);
    void to_json(json& j, const FrequencyRange& value);
    void from_json(const json& j, FrequencyRange& value);
    void to_json(json& j, const GeoPoint& value);
    void from_json(const json& j, GeoPoint& value);
    void to_json(json& j, const SourceRef& value);
    void from_json(const json& j, SourceRef& value);
    void to_json(json& j, SegmentKind value);
    void from_json(const json& j, SegmentKind& value);
    void to_json(json& j, AllocationStatus value);
    void from_json(const json& j, AllocationStatus& value);
    void to_json(json& j, const PlanScope& value);
    void from_json(const json& j, PlanScope& value);
    void to_json(json& j, const BandPlan& value);
    void from_json(const json& j, BandPlan& value);
    void to_json(json& j, const Band& value);
    void from_json(const json& j, Band& value);
    void to_json(json& j, const Segment& value);
    void from_json(const json& j, Segment& value);
    void to_json(json& j, const WeeklySchedule& value);
    void from_json(const json& j, WeeklySchedule& value);
    void to_json(json& j, const Bookmark& value);
    void from_json(const json& j, Bookmark& value);
    void to_json(json& j, const EibiScheduleRecord& value);
    void from_json(const json& j, EibiScheduleRecord& value);
    void to_json(json& j, const RepeaterRecord& value);
    void from_json(const json& j, RepeaterRecord& value);
    void to_json(json& j, const CatalogDocument& value);
    void from_json(const json& j, CatalogDocument& value);

}

namespace std {
    template <typename Tag>
    struct hash<frequency_catalog::Id<Tag>> {
        size_t operator()(const frequency_catalog::Id<Tag>& id) const noexcept {
            return hash<std::string>{}(id.str());
        }
    };
}

namespace frequency_catalog {
    template <typename Tag>
    void from_json(const json& j, Id<Tag>& id) {
        std::string value = j.get<std::string>();
        if (!isValidStableId(value)) {
            throw std::invalid_argument("invalid frequency catalog stable id: " + value);
        }
        id = Id<Tag>(std::move(value));
    }
}
