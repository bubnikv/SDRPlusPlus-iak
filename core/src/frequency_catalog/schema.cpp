#include "schema.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

namespace frequency_catalog {

    namespace {
        constexpr uint64_t FNV_PRIME = 1099511628211ULL;
        constexpr uint64_t FNV_OFFSET_1 = 14695981039346656037ULL;
        constexpr uint64_t FNV_OFFSET_2 = 7809847782465536322ULL;

        void hashByte(uint64_t& state, uint8_t value) {
            state ^= value;
            state *= FNV_PRIME;
        }

        void hashUint64(uint64_t& state, uint64_t value) {
            for (int i = 0; i < 8; i++) {
                hashByte(state, static_cast<uint8_t>(value & 0xFF));
                value >>= 8;
            }
        }

        void hashPart(uint64_t& state, std::string_view value) {
            hashUint64(state, value.size());
            for (unsigned char c : value) {
                hashByte(state, c);
            }
        }

        bool isFiniteNonNegative(double value) {
            return std::isfinite(value) && value >= 0.0;
        }

        bool validMode(int mode) {
            return mode >= -1 && mode < _RADIO_IFACE_MODE_COUNT;
        }

        int modeFromJson(const json& j, const char* key, int defaultMode) {
            auto it = j.find(key);
            if (it == j.end() || it->is_null()) {
                return defaultMode;
            }
            std::string name = it->get<std::string>();
            int mode = radioModeFromName(name.c_str());
            if (mode < 0) {
                throw std::invalid_argument(std::string("unknown radio mode in ") + key + ": " + name);
            }
            return mode;
        }

        bool validYmd(int ymd) {
            if (ymd == 0) {
                return true;
            }
            int year = ymd / 10000;
            int month = (ymd / 100) % 100;
            int day = ymd % 100;
            if (year < 1900 || month < 1 || month > 12 || day < 1) {
                return false;
            }
            static constexpr int DAYS_IN_MONTH[] = {
                31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
            };
            int maxDay = DAYS_IN_MONTH[month - 1];
            bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
            if (month == 2 && leap) {
                maxDay = 29;
            }
            return day <= maxDay;
        }

        void validateRange(
            const FrequencyRange& range,
            const std::string& path,
            std::vector<std::string>& errors) {
            if (!isFiniteNonNegative(range.minHz) || !isFiniteNonNegative(range.maxHz)) {
                errors.push_back(path + " must contain finite, non-negative frequencies");
            }
            else if (range.maxHz < range.minHz) {
                errors.push_back(path + ".max_hz is below min_hz");
            }
        }

        void validateMode(int mode, const std::string& path, std::vector<std::string>& errors) {
            if (!validMode(mode)) {
                errors.push_back(path + " is not RADIO_IFACE_MODE_* or -1");
            }
        }

        void validateSchedule(
            const WeeklySchedule& schedule,
            const std::string& path,
            std::vector<std::string>& errors) {
            if (schedule.startMinuteUtc < 0 || schedule.startMinuteUtc > 1439) {
                errors.push_back(path + ".start_minute_utc is outside 0..1439");
            }
            if (schedule.endMinuteUtc < 0 || schedule.endMinuteUtc > 1440) {
                errors.push_back(path + ".end_minute_utc is outside 0..1440");
            }
            if ((schedule.dayMask & ~0x7F) != 0) {
                errors.push_back(path + ".day_mask contains bits outside Sunday..Saturday");
            }
            if (!validYmd(schedule.validFromYmd)) {
                errors.push_back(path + ".valid_from_ymd is not a valid YYYYMMDD date or 0");
            }
            if (!validYmd(schedule.validUntilYmd)) {
                errors.push_back(path + ".valid_until_ymd is not a valid YYYYMMDD date or 0");
            }
            if (schedule.validFromYmd > 0 && schedule.validUntilYmd > 0
                && schedule.validUntilYmd < schedule.validFromYmd) {
                errors.push_back(path + ".valid_until_ymd precedes valid_from_ymd");
            }
        }

        void validateLocation(
            const GeoPoint& location,
            const std::string& path,
            std::vector<std::string>& errors) {
            if (!std::isfinite(location.latitude) || location.latitude < -90.0 || location.latitude > 90.0) {
                errors.push_back(path + ".latitude is outside -90..90");
            }
            if (!std::isfinite(location.longitude) || location.longitude < -180.0 || location.longitude > 180.0) {
                errors.push_back(path + ".longitude is outside -180..180");
            }
        }

        void validateSourceRef(
            const SourceRef& source,
            const std::string& path,
            std::vector<std::string>& errors) {
            if (!isValidProviderName(source.provider)) {
                errors.push_back(path + ".provider is invalid");
            }
            if (!isValidStableId(source.recordId.str())) {
                errors.push_back(path + ".record_id is invalid");
            }
            else if (source.recordId.str().rfind(source.provider + ":", 0) != 0) {
                errors.push_back(path + ".record_id must use the provider namespace");
            }
        }

        template <typename IdType>
        bool insertUnique(
            std::unordered_set<IdType>& ids,
            const IdType& id,
            const std::string& path,
            std::vector<std::string>& errors) {
            if (!isValidStableId(id.str())) {
                errors.push_back(path + " is invalid");
                return false;
            }
            if (!ids.insert(id).second) {
                errors.push_back(path + " is duplicated: " + id.str());
                return false;
            }
            return true;
        }

        template <typename T>
        void putOptional(json& j, const char* key, const std::optional<T>& value) {
            if (value) {
                j[key] = *value;
            }
        }

        template <typename T>
        std::optional<T> getOptional(const json& j, const char* key) {
            auto it = j.find(key);
            if (it == j.end() || it->is_null()) {
                return std::nullopt;
            }
            return it->get<T>();
        }

        using CompactKeyMap =
            std::initializer_list<std::pair<const char*, const char*>>;

        json expandCompactObject(
            const json& compact,
            CompactKeyMap keys,
            const std::string& path) {
            if (!compact.is_object()) {
                throw std::invalid_argument(path + " must be a CBOR map");
            }
            json expanded = json::object();
            for (auto it = compact.begin(); it != compact.end(); ++it) {
                const char* longKey = nullptr;
                for (const auto& [candidateLong, candidateShort] : keys) {
                    if (it.key() == candidateShort) {
                        longKey = candidateLong;
                        break;
                    }
                }
                if (!longKey) {
                    throw std::invalid_argument(
                        path + " contains unknown compact key: " + it.key());
                }
                if (expanded.contains(longKey)) {
                    throw std::invalid_argument(
                        path + " expands to duplicate key: " + longKey);
                }
                expanded[longKey] = it.value();
            }
            return expanded;
        }

        void requireArray(const json& value, const std::string& path) {
            if (!value.is_array()) {
                throw std::invalid_argument(path + " must be a CBOR array");
            }
        }

        json expandCompactScope(const json& compact, const std::string& path) {
            return expandCompactObject(
                compact,
                {
                    { "itu_region_mask", "i" },
                    { "country_codes", "c" },
                    { "subdivisions", "s" }
                },
                path);
        }

        json expandCompactSourceRef(const json& compact, const std::string& path) {
            return expandCompactObject(
                compact,
                {
                    { "provider", "p" },
                    { "record_id", "i" },
                    { "upstream_id", "u" },
                    { "url", "l" }
                },
                path);
        }

        json expandCompactSchedule(const json& compact, const std::string& path) {
            return expandCompactObject(
                compact,
                {
                    { "start_minute_utc", "s" },
                    { "end_minute_utc", "e" },
                    { "day_mask", "d" },
                    { "valid_from_ymd", "f" },
                    { "valid_until_ymd", "u" },
                    { "days_text", "t" }
                },
                path);
        }

        json expandCompactCatalog(const json& encoded) {
            if (!encoded.is_object()) {
                throw std::invalid_argument("CBOR catalog root must be a map");
            }
            auto wireIt = encoded.find("w");
            if (wireIt == encoded.end() || !wireIt->is_number_integer()
                || wireIt->get<int>() != CATALOG_CBOR_WIRE_SCHEMA_VERSION) {
                throw std::invalid_argument("unsupported frequency catalog CBOR wire schema");
            }

            json compact = encoded;
            compact.erase("w");
            json document = expandCompactObject(
                compact,
                {
                    { "schema_version", "v" },
                    { "plans", "p" },
                    { "bands", "b" },
                    { "segments", "s" },
                    { "bookmarks", "m" }
                },
                "cbor");

            requireArray(document.at("plans"), "cbor.p");
            json plans = json::array();
            for (size_t i = 0; i < document["plans"].size(); i++) {
                const json& compactPlan = document["plans"][i];
                json plan = expandCompactObject(
                    compactPlan,
                    {
                        { "plan_id", "i" },
                        { "name", "n" },
                        { "scope", "c" },
                        { "source", "o" },
                        { "revision", "r" }
                    },
                    "cbor.p[" + std::to_string(i) + "]");
                if (compactPlan.contains("c")) {
                    plan["scope"] = expandCompactScope(
                        compactPlan.at("c"),
                        "cbor.p[" + std::to_string(i) + "].c");
                }
                plans.push_back(std::move(plan));
            }
            document["plans"] = std::move(plans);

            requireArray(document.at("bands"), "cbor.b");
            json bands = json::array();
            for (size_t i = 0; i < document["bands"].size(); i++) {
                bands.push_back(expandCompactObject(
                    document["bands"][i],
                    {
                        { "band_id", "i" },
                        { "name", "n" },
                        { "service", "s" }
                    },
                    "cbor.b[" + std::to_string(i) + "]"));
            }
            document["bands"] = std::move(bands);

            requireArray(document.at("segments"), "cbor.s");
            json segments = json::array();
            for (size_t i = 0; i < document["segments"].size(); i++) {
                const json& compactSegment = document["segments"][i];
                json segment = expandCompactObject(
                    compactSegment,
                    {
                        { "segment_id", "i" },
                        { "plan_id", "p" },
                        { "band_id", "b" },
                        { "name", "n" },
                        { "service", "s" },
                        { "kind", "k" },
                        { "status", "t" },
                        { "range", "r" },
                        { "default_frequency", "f" },
                        { "default_mode", "m" },
                        { "channel_spacing", "c" }
                    },
                    "cbor.s[" + std::to_string(i) + "]");
                segment["range"] = expandCompactObject(
                    compactSegment.at("r"),
                    {
                        { "min_hz", "l" },
                        { "max_hz", "h" }
                    },
                    "cbor.s[" + std::to_string(i) + "].r");
                segments.push_back(std::move(segment));
            }
            document["segments"] = std::move(segments);

            requireArray(document.at("bookmarks"), "cbor.m");
            json bookmarks = json::array();
            for (size_t i = 0; i < document["bookmarks"].size(); i++) {
                const json& compactBookmark = document["bookmarks"][i];
                const std::string path = "cbor.m[" + std::to_string(i) + "]";
                json bookmark = expandCompactObject(
                    compactBookmark,
                    {
                        { "bookmark_id", "i" },
                        { "layer", "l" },
                        { "band_id", "b" },
                        { "scope", "s" },
                        { "name", "n" },
                        { "frequency", "f" },
                        { "bandwidth", "w" },
                        { "mode", "m" },
                        { "source_mode", "d" },
                        { "underlying_mode", "u" },
                        { "scannable", "c" },
                        { "schedule", "h" },
                        { "notes", "o" },
                        { "geo_info", "g" },
                        { "source_ref", "r" }
                    },
                    path);
                if (compactBookmark.contains("s")) {
                    bookmark["scope"] =
                        expandCompactScope(compactBookmark.at("s"), path + ".s");
                }
                if (compactBookmark.contains("r")) {
                    bookmark["source_ref"] =
                        expandCompactSourceRef(compactBookmark.at("r"), path + ".r");
                }
                if (compactBookmark.contains("h")) {
                    bookmark["schedule"] =
                        expandCompactSchedule(compactBookmark.at("h"), path + ".h");
                }
                bookmarks.push_back(std::move(bookmark));
            }
            document["bookmarks"] = std::move(bookmarks);
            return document;
        }
    }

    bool isValidStableId(std::string_view value) {
        if (value.empty() || value.size() > 128) {
            return false;
        }
        auto isLowerAlpha = [](char c) { return c >= 'a' && c <= 'z'; };
        auto isDigit = [](char c) { return c >= '0' && c <= '9'; };
        if (!isLowerAlpha(value.front()) && !isDigit(value.front())) {
            return false;
        }
        for (char c : value) {
            if (!isLowerAlpha(c) && !isDigit(c)
                && c != '.' && c != '_' && c != '-' && c != ':') {
                return false;
            }
        }
        return true;
    }

    bool isValidProviderName(std::string_view value) {
        if (value.empty() || value.size() > 32) {
            return false;
        }
        for (char c : value) {
            if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')
                || c == '_' || c == '-')) {
                return false;
            }
        }
        return value.front() >= 'a' && value.front() <= 'z';
    }

    ProviderRecordId makeProviderRecordId(
        std::string_view provider,
        std::initializer_list<std::string_view> normalizedParts) {
        if (!isValidProviderName(provider)) {
            throw std::invalid_argument("invalid frequency catalog provider name");
        }

        uint64_t first = FNV_OFFSET_1;
        uint64_t second = FNV_OFFSET_2;
        hashPart(first, provider);
        hashPart(second, provider);
        for (std::string_view part : normalizedParts) {
            hashPart(first, part);
            hashPart(second, part);
        }

        std::ostringstream id;
        id << provider << ':' << std::hex << std::setfill('0')
           << std::setw(16) << first << std::setw(16) << second;
        return ProviderRecordId(id.str());
    }

    std::vector<std::string> validate(const CatalogDocument& document) {
        std::vector<std::string> errors;
        if (document.schemaVersion != CATALOG_SCHEMA_VERSION) {
            errors.push_back("schema_version is not the current catalog schema");
        }

        std::unordered_set<PlanId> planIds;
        for (size_t i = 0; i < document.plans.size(); i++) {
            const BandPlan& plan = document.plans[i];
            const std::string path = "plans[" + std::to_string(i) + "]";
            insertUnique(planIds, plan.planId, path + ".plan_id", errors);
            if (plan.name.empty()) {
                errors.push_back(path + ".name is empty");
            }
            if ((plan.scope.ituRegionMask & ~0x07) != 0) {
                errors.push_back(path + ".scope.itu_region_mask contains bits outside Regions 1..3");
            }
        }

        std::unordered_set<BandId> bandIds;
        for (size_t i = 0; i < document.bands.size(); i++) {
            const Band& band = document.bands[i];
            const std::string path = "bands[" + std::to_string(i) + "]";
            insertUnique(bandIds, band.bandId, path + ".band_id", errors);
            if (band.name.empty()) {
                errors.push_back(path + ".name is empty");
            }
        }

        std::unordered_set<SegmentId> segmentIds;
        for (size_t i = 0; i < document.segments.size(); i++) {
            const Segment& segment = document.segments[i];
            const std::string path = "segments[" + std::to_string(i) + "]";
            insertUnique(segmentIds, segment.segmentId, path + ".segment_id", errors);
            // References may resolve in the other static layer. The catalog
            // validates them after merging.
            if (!isValidStableId(segment.planId.str())) {
                errors.push_back(path + ".plan_id is invalid");
            }
            if (segment.bandId && !isValidStableId(segment.bandId->str())) {
                errors.push_back(path + ".band_id is invalid");
            }
            if (segment.name.empty()) {
                errors.push_back(path + ".name is empty");
            }
            validateRange(segment.range, path + ".range", errors);
            if (!isFiniteNonNegative(segment.defaultFrequency)) {
                errors.push_back(path + ".default_frequency must be finite and non-negative");
            }
            else if (segment.defaultFrequency > 0.0 && !segment.range.contains(segment.defaultFrequency)) {
                errors.push_back(path + ".default_frequency is outside the Segment range");
            }
            if (!isFiniteNonNegative(segment.channelSpacing)) {
                errors.push_back(path + ".channel_spacing must be finite and non-negative");
            }
            validateMode(segment.defaultMode, path + ".default_mode", errors);
            if (segment.sourceRef) {
                validateSourceRef(*segment.sourceRef, path + ".source_ref", errors);
            }
        }

        std::unordered_set<BookmarkId> bookmarkIds;
        for (size_t i = 0; i < document.bookmarks.size(); i++) {
            const Bookmark& bookmark = document.bookmarks[i];
            const std::string path = "bookmarks[" + std::to_string(i) + "]";
            insertUnique(bookmarkIds, bookmark.bookmarkId, path + ".bookmark_id", errors);
            if (bookmark.layer == CatalogLayer::Dynamic) {
                errors.push_back(path + ".layer cannot be dynamic in a persisted catalog document");
            }
            // User bookmarks are normally persisted separately from the
            // system Band registry, so this reference may resolve in another
            // loaded layer. Syntax is still checked here; the catalog service
            // will resolve it against the merged Band registry.
            if (bookmark.bandId && !isValidStableId(bookmark.bandId->str())) {
                errors.push_back(path + ".band_id is invalid");
            }
            if ((bookmark.scope.ituRegionMask & ~0x07) != 0) {
                errors.push_back(path + ".scope.itu_region_mask contains bits outside Regions 1..3");
            }
            if (bookmark.name.empty()) {
                errors.push_back(path + ".name is empty");
            }
            if (!std::isfinite(bookmark.frequency) || bookmark.frequency <= 0.0) {
                errors.push_back(path + ".frequency must be finite and positive");
            }
            if (!isFiniteNonNegative(bookmark.bandwidth)) {
                errors.push_back(path + ".bandwidth must be finite and non-negative");
            }
            validateMode(bookmark.mode, path + ".mode", errors);
            if (bookmark.schedule) {
                validateSchedule(*bookmark.schedule, path + ".schedule", errors);
            }
            if (bookmark.sourceRef) {
                validateSourceRef(*bookmark.sourceRef, path + ".source_ref", errors);
            }
        }
        return errors;
    }

    std::vector<std::string> validate(const EibiScheduleRecord& record) {
        std::vector<std::string> errors;
        validateSourceRef(record.sourceRef, "source_ref", errors);
        if (record.bandId && !isValidStableId(record.bandId->str())) {
            errors.push_back("band_id is invalid");
        }
        if (record.station.empty()) {
            errors.push_back("station is empty");
        }
        if (!std::isfinite(record.carrierFrequency) || record.carrierFrequency <= 0.0) {
            errors.push_back("carrier_frequency must be finite and positive");
        }
        if (!std::isfinite(record.tuningFrequency) || record.tuningFrequency <= 0.0) {
            errors.push_back("tuning_frequency must be finite and positive");
        }
        validateMode(record.mode, "mode", errors);
        validateSchedule(record.schedule, "schedule", errors);
        if (record.persistenceCode < 0 || record.persistenceCode > 99) {
            errors.push_back("persistence_code is outside 0..99");
        }
        if (record.transmitterLocation) {
            validateLocation(*record.transmitterLocation, "transmitter_location", errors);
        }
        return errors;
    }

    std::vector<std::string> validate(const RepeaterRecord& record) {
        std::vector<std::string> errors;
        validateSourceRef(record.sourceRef, "source_ref", errors);
        if (record.bandId && !isValidStableId(record.bandId->str())) {
            errors.push_back("band_id is invalid");
        }
        if (record.callsign.empty() && record.name.empty()) {
            errors.push_back("callsign and name are both empty");
        }
        if (!std::isfinite(record.outputFrequency) || record.outputFrequency <= 0.0) {
            errors.push_back("output_frequency must be finite and positive");
        }
        if (!isFiniteNonNegative(record.inputFrequency)) {
            errors.push_back("input_frequency must be finite and non-negative");
        }
        std::unordered_set<std::string> modes;
        for (size_t i = 0; i < record.modes.size(); i++) {
            if (record.modes[i].empty()) {
                errors.push_back("modes[" + std::to_string(i) + "] is empty");
            }
            else if (!modes.insert(record.modes[i]).second) {
                errors.push_back("modes contains a duplicate capability: " + record.modes[i]);
            }
        }
        validateMode(record.tuningMode, "tuning_mode", errors);
        validateLocation(record.location, "location", errors);
        return errors;
    }

    bool migrateCatalogDocument(json& document, std::string& error) {
        if (!document.is_object()) {
            error = "catalog document must be a JSON object";
            return false;
        }
        auto versionIt = document.find("schema_version");
        if (versionIt == document.end() || !versionIt->is_number_integer()) {
            error = "catalog document has no integer schema_version; use a legacy import adapter";
            return false;
        }

        int version = versionIt->get<int>();
        if (version > CATALOG_SCHEMA_VERSION) {
            error = "catalog schema is newer than this application supports";
            return false;
        }
        if (version < 1) {
            error = "catalog schema has no registered migration path";
            return false;
        }

        // Future migrations are applied one version at a time here:
        //
        // while (version < CATALOG_SCHEMA_VERSION) {
        //     switch (version) {
        //         case 1: migrateV1ToV2(document); version = 2; break;
        //         ...
        //     }
        // }
        //
        // Keeping this entry point in version 1 means every reader follows the
        // same checked path before a second schema version is introduced.
        error.clear();
        return version == CATALOG_SCHEMA_VERSION;
    }

    CatalogDocument catalogDocumentFromJson(json document) {
        std::string error;
        if (!migrateCatalogDocument(document, error)) {
            throw std::invalid_argument(error);
        }
        CatalogDocument result = document.get<CatalogDocument>();
        std::vector<std::string> errors = validate(result);
        if (!errors.empty()) {
            throw std::invalid_argument("invalid frequency catalog: " + errors.front());
        }
        return result;
    }

    CatalogDocument catalogDocumentFromCbor(const std::vector<uint8_t>& data) {
        json compact;
        try {
            compact = json::from_cbor(data);
        }
        catch (const json::exception& error) {
            throw std::invalid_argument(
                std::string("invalid frequency catalog CBOR: ") + error.what());
        }
        return catalogDocumentFromJson(expandCompactCatalog(compact));
    }

    json catalogDocumentToJson(const CatalogDocument& document) {
        std::vector<std::string> errors = validate(document);
        if (!errors.empty()) {
            throw std::invalid_argument("invalid frequency catalog: " + errors.front());
        }
        return json(document);
    }

    void to_json(json& j, CatalogLayer layer) {
        switch (layer) {
            case CatalogLayer::System: j = "system"; return;
            case CatalogLayer::User: j = "user"; return;
            case CatalogLayer::Dynamic: j = "dynamic"; return;
        }
        throw std::invalid_argument("unknown frequency catalog layer");
    }

    void from_json(const json& j, CatalogLayer& layer) {
        const std::string value = j.get<std::string>();
        if (value == "system") { layer = CatalogLayer::System; }
        else if (value == "user") { layer = CatalogLayer::User; }
        else if (value == "dynamic") { layer = CatalogLayer::Dynamic; }
        else { throw std::invalid_argument("unknown frequency catalog layer: " + value); }
    }

    void to_json(json& j, const FrequencyRange& value) {
        j = { { "min_hz", value.minHz }, { "max_hz", value.maxHz } };
    }

    void from_json(const json& j, FrequencyRange& value) {
        j.at("min_hz").get_to(value.minHz);
        j.at("max_hz").get_to(value.maxHz);
    }

    void to_json(json& j, const GeoPoint& value) {
        j = { { "latitude", value.latitude }, { "longitude", value.longitude } };
    }

    void from_json(const json& j, GeoPoint& value) {
        j.at("latitude").get_to(value.latitude);
        j.at("longitude").get_to(value.longitude);
    }

    void to_json(json& j, const SourceRef& value) {
        j = {
            { "provider", value.provider },
            { "record_id", value.recordId }
        };
        if (!value.upstreamId.empty()) { j["upstream_id"] = value.upstreamId; }
        if (!value.url.empty()) { j["url"] = value.url; }
    }

    void from_json(const json& j, SourceRef& value) {
        j.at("provider").get_to(value.provider);
        j.at("record_id").get_to(value.recordId);
        value.upstreamId = j.value("upstream_id", "");
        value.url = j.value("url", "");
    }

    void to_json(json& j, SegmentKind value) {
        switch (value) {
            case SegmentKind::Allocation: j = "allocation"; return;
            case SegmentKind::OperatingPlan: j = "operating_plan"; return;
            case SegmentKind::Usage: j = "usage"; return;
            case SegmentKind::Application: j = "application"; return;
            case SegmentKind::ChannelPlan: j = "channel_plan"; return;
        }
        throw std::invalid_argument("unknown segment kind");
    }

    void from_json(const json& j, SegmentKind& value) {
        const std::string text = j.get<std::string>();
        if (text == "allocation") { value = SegmentKind::Allocation; }
        else if (text == "operating_plan") { value = SegmentKind::OperatingPlan; }
        else if (text == "usage") { value = SegmentKind::Usage; }
        else if (text == "application") { value = SegmentKind::Application; }
        else if (text == "channel_plan") { value = SegmentKind::ChannelPlan; }
        else { throw std::invalid_argument("unknown segment kind: " + text); }
    }

    void to_json(json& j, AllocationStatus value) {
        switch (value) {
            case AllocationStatus::Unspecified: j = "unspecified"; return;
            case AllocationStatus::Primary: j = "primary"; return;
            case AllocationStatus::Secondary: j = "secondary"; return;
            case AllocationStatus::Advisory: j = "advisory"; return;
        }
        throw std::invalid_argument("unknown allocation status");
    }

    void from_json(const json& j, AllocationStatus& value) {
        const std::string text = j.get<std::string>();
        if (text == "unspecified") { value = AllocationStatus::Unspecified; }
        else if (text == "primary") { value = AllocationStatus::Primary; }
        else if (text == "secondary") { value = AllocationStatus::Secondary; }
        else if (text == "advisory") { value = AllocationStatus::Advisory; }
        else { throw std::invalid_argument("unknown allocation status: " + text); }
    }

    void to_json(json& j, const PlanScope& value) {
        j = json::object();
        if (value.ituRegionMask != 0) { j["itu_region_mask"] = value.ituRegionMask; }
        if (!value.countryCodes.empty()) { j["country_codes"] = value.countryCodes; }
        if (!value.subdivisions.empty()) { j["subdivisions"] = value.subdivisions; }
    }

    void from_json(const json& j, PlanScope& value) {
        value.ituRegionMask = j.value("itu_region_mask", 0);
        value.countryCodes = j.value("country_codes", std::vector<std::string>{});
        value.subdivisions = j.value("subdivisions", std::vector<std::string>{});
    }

    void to_json(json& j, const BandPlan& value) {
        j = {
            { "plan_id", value.planId },
            { "name", value.name },
            { "scope", value.scope }
        };
        if (!value.source.empty()) { j["source"] = value.source; }
        if (!value.revision.empty()) { j["revision"] = value.revision; }
    }

    void from_json(const json& j, BandPlan& value) {
        j.at("plan_id").get_to(value.planId);
        j.at("name").get_to(value.name);
        value.scope = j.value("scope", PlanScope{});
        value.source = j.value("source", "");
        value.revision = j.value("revision", "");
    }

    void to_json(json& j, const Band& value) {
        j = {
            { "band_id", value.bandId },
            { "name", value.name }
        };
        if (!value.service.empty()) { j["service"] = value.service; }
    }

    void from_json(const json& j, Band& value) {
        j.at("band_id").get_to(value.bandId);
        j.at("name").get_to(value.name);
        value.service = j.value("service", "");
    }

    void to_json(json& j, const Segment& value) {
        j = {
            { "segment_id", value.segmentId },
            { "plan_id", value.planId },
            { "name", value.name },
            { "service", value.service },
            { "kind", value.kind },
            { "status", value.status },
            { "range", value.range }
        };
        putOptional(j, "band_id", value.bandId);
        if (value.defaultFrequency > 0.0) { j["default_frequency"] = value.defaultFrequency; }
        if (value.defaultMode >= 0) { j["default_mode"] = radioModeName(value.defaultMode); }
        if (value.channelSpacing > 0.0) { j["channel_spacing"] = value.channelSpacing; }
        putOptional(j, "source_ref", value.sourceRef);
    }

    void from_json(const json& j, Segment& value) {
        j.at("segment_id").get_to(value.segmentId);
        j.at("plan_id").get_to(value.planId);
        value.bandId = getOptional<BandId>(j, "band_id");
        j.at("name").get_to(value.name);
        value.service = j.value("service", "");
        value.kind = j.value("kind", SegmentKind::Allocation);
        value.status = j.value("status", AllocationStatus::Unspecified);
        j.at("range").get_to(value.range);
        value.defaultFrequency = j.value("default_frequency", 0.0);
        value.defaultMode = modeFromJson(j, "default_mode", -1);
        value.channelSpacing = j.value("channel_spacing", 0.0);
        value.sourceRef = getOptional<SourceRef>(j, "source_ref");
    }

    void to_json(json& j, const WeeklySchedule& value) {
        j = {
            { "start_minute_utc", value.startMinuteUtc },
            { "end_minute_utc", value.endMinuteUtc },
            { "day_mask", value.dayMask }
        };
        if (value.validFromYmd > 0) { j["valid_from_ymd"] = value.validFromYmd; }
        if (value.validUntilYmd > 0) { j["valid_until_ymd"] = value.validUntilYmd; }
        if (!value.daysText.empty()) { j["days_text"] = value.daysText; }
    }

    void from_json(const json& j, WeeklySchedule& value) {
        value.startMinuteUtc = j.value("start_minute_utc", 0);
        value.endMinuteUtc = j.value("end_minute_utc", 0);
        value.dayMask = j.value("day_mask", 0x7F);
        value.validFromYmd = j.value("valid_from_ymd", 0);
        value.validUntilYmd = j.value("valid_until_ymd", 0);
        value.daysText = j.value("days_text", "");
    }

    void to_json(json& j, const Bookmark& value) {
        j = {
            { "bookmark_id", value.bookmarkId },
            { "layer", value.layer },
            { "name", value.name },
            { "frequency", value.frequency },
            { "bandwidth", value.bandwidth },
            { "scope", value.scope }
        };
        putOptional(j, "band_id", value.bandId);
        if (value.mode >= 0) { j["mode"] = radioModeName(value.mode); }
        if (!value.sourceMode.empty()) { j["source_mode"] = value.sourceMode; }
        if (!value.underlyingMode.empty()) { j["underlying_mode"] = value.underlyingMode; }
        if (!value.scannable) { j["scannable"] = false; }
        putOptional(j, "schedule", value.schedule);
        if (!value.notes.empty()) { j["notes"] = value.notes; }
        if (!value.geoInfo.empty()) { j["geo_info"] = value.geoInfo; }
        putOptional(j, "source_ref", value.sourceRef);
    }

    void from_json(const json& j, Bookmark& value) {
        j.at("bookmark_id").get_to(value.bookmarkId);
        value.layer = j.value("layer", CatalogLayer::User);
        value.bandId = getOptional<BandId>(j, "band_id");
        value.scope = j.value("scope", PlanScope{});
        j.at("name").get_to(value.name);
        j.at("frequency").get_to(value.frequency);
        value.bandwidth = j.value("bandwidth", 0.0);
        value.mode = modeFromJson(j, "mode", -1);
        value.sourceMode = j.value("source_mode", "");
        value.underlyingMode = j.value("underlying_mode", "");
        value.scannable = j.value("scannable", true);
        value.schedule = getOptional<WeeklySchedule>(j, "schedule");
        value.notes = j.value("notes", "");
        value.geoInfo = j.value("geo_info", "");
        value.sourceRef = getOptional<SourceRef>(j, "source_ref");
    }

    void to_json(json& j, const EibiScheduleRecord& value) {
        j = {
            { "source_ref", value.sourceRef },
            { "station", value.station },
            { "country_code", value.countryCode },
            { "language", value.language },
            { "target", value.target },
            { "transmitter_site_code", value.transmitterSiteCode },
            { "transmitter_site", value.transmitterSite },
            { "remarks", value.remarks },
            { "persistence_code", value.persistenceCode },
            { "start_date_text", value.startDateText },
            { "stop_date_text", value.stopDateText },
            { "carrier_frequency", value.carrierFrequency },
            { "tuning_frequency", value.tuningFrequency },
            { "mode", radioModeName(value.mode) },
            { "schedule", value.schedule }
        };
        putOptional(j, "band_id", value.bandId);
        putOptional(j, "transmitter_location", value.transmitterLocation);
    }

    void from_json(const json& j, EibiScheduleRecord& value) {
        j.at("source_ref").get_to(value.sourceRef);
        value.bandId = getOptional<BandId>(j, "band_id");
        j.at("station").get_to(value.station);
        value.countryCode = j.value("country_code", "");
        value.language = j.value("language", "");
        value.target = j.value("target", "");
        value.transmitterSiteCode = j.value("transmitter_site_code", "");
        value.transmitterSite = j.value("transmitter_site", "");
        value.remarks = j.value("remarks", "");
        value.persistenceCode = j.value("persistence_code", 0);
        value.startDateText = j.value("start_date_text", "");
        value.stopDateText = j.value("stop_date_text", "");
        j.at("carrier_frequency").get_to(value.carrierFrequency);
        value.tuningFrequency = j.value("tuning_frequency", value.carrierFrequency);
        value.mode = modeFromJson(j, "mode", RADIO_IFACE_MODE_AM);
        j.at("schedule").get_to(value.schedule);
        value.transmitterLocation = getOptional<GeoPoint>(j, "transmitter_location");
    }

    void to_json(json& j, const RepeaterRecord& value) {
        j = {
            { "source_ref", value.sourceRef },
            { "callsign", value.callsign },
            { "name", value.name },
            { "output_frequency", value.outputFrequency },
            { "input_frequency", value.inputFrequency },
            { "modes", value.modes },
            { "location", value.location }
        };
        putOptional(j, "band_id", value.bandId);
        if (value.tuningMode >= 0) { j["tuning_mode"] = radioModeName(value.tuningMode); }
        if (!value.uplinkTone.empty()) { j["uplink_tone"] = value.uplinkTone; }
        if (!value.downlinkTone.empty()) { j["downlink_tone"] = value.downlinkTone; }
        if (!value.status.empty()) { j["status"] = value.status; }
        if (!value.lastUpdated.empty()) { j["last_updated"] = value.lastUpdated; }
        if (!value.notes.empty()) { j["notes"] = value.notes; }
    }

    void from_json(const json& j, RepeaterRecord& value) {
        j.at("source_ref").get_to(value.sourceRef);
        value.bandId = getOptional<BandId>(j, "band_id");
        value.callsign = j.value("callsign", "");
        value.name = j.value("name", "");
        j.at("output_frequency").get_to(value.outputFrequency);
        value.inputFrequency = j.value("input_frequency", 0.0);
        value.modes = j.value("modes", std::vector<std::string>{});
        value.tuningMode = modeFromJson(j, "tuning_mode", -1);
        value.uplinkTone = j.value("uplink_tone", "");
        value.downlinkTone = j.value("downlink_tone", "");
        value.status = j.value("status", "");
        value.lastUpdated = j.value("last_updated", "");
        value.notes = j.value("notes", "");
        j.at("location").get_to(value.location);
    }

    void to_json(json& j, const CatalogDocument& value) {
        j = {
            { "schema_version", value.schemaVersion },
            { "plans", value.plans },
            { "bands", value.bands },
            { "segments", value.segments },
            { "bookmarks", value.bookmarks }
        };
    }

    void from_json(const json& j, CatalogDocument& value) {
        j.at("schema_version").get_to(value.schemaVersion);
        value.plans = j.value("plans", std::vector<BandPlan>{});
        value.bands = j.value("bands", std::vector<Band>{});
        value.segments = j.value("segments", std::vector<Segment>{});
        value.bookmarks = j.value("bookmarks", std::vector<Bookmark>{});
    }

}
