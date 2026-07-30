#include "catalog.h"

#include <core.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace frequency_catalog {

    namespace {
        template <typename Entity, typename IdType, typename GetId>
        std::vector<Entity> mergeById(
            const std::vector<Entity>& systemEntities,
            const std::vector<Entity>& userEntities,
            GetId getId) {
            std::vector<Entity> merged = systemEntities;
            std::unordered_map<IdType, size_t> positions;
            positions.reserve(merged.size() + userEntities.size());
            for (size_t i = 0; i < merged.size(); i++) {
                positions[getId(merged[i])] = i;
            }
            for (const Entity& entity : userEntities) {
                const IdType& id = getId(entity);
                auto existing = positions.find(id);
                if (existing == positions.end()) {
                    positions[id] = merged.size();
                    merged.push_back(entity);
                }
                else {
                    merged[existing->second] = entity;
                }
            }
            return merged;
        }

        template <typename Entity, typename Frequency>
        void sortFrequencyOrder(
            const std::vector<Entity>& entities,
            std::vector<size_t>& order,
            Frequency frequency) {
            order.resize(entities.size());
            for (size_t i = 0; i < entities.size(); i++) {
                order[i] = i;
            }
            std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) {
                return frequency(entities[a]) < frequency(entities[b]);
            });
        }

        template <typename Entity, typename Frequency>
        std::vector<const Entity*> queryFrequencyOrder(
            const std::vector<Entity>& entities,
            const std::vector<size_t>& order,
            double minFrequency,
            double maxFrequency,
            Frequency frequency) {
            auto first = std::lower_bound(order.begin(), order.end(), minFrequency,
                [&](size_t index, double value) {
                    return frequency(entities[index]) < value;
                });
            auto last = std::upper_bound(first, order.end(), maxFrequency,
                [&](double value, size_t index) {
                    return value < frequency(entities[index]);
                });
            std::vector<const Entity*> result;
            result.reserve(static_cast<size_t>(last - first));
            for (auto it = first; it != last; ++it) {
                result.push_back(&entities[*it]);
            }
            return result;
        }

        bool rangesOverlap(const FrequencyRange& range, double minFrequency, double maxFrequency) {
            return range.maxHz >= minFrequency && range.minHz <= maxFrequency;
        }

        bool planActive(const PlanId& planId, const CatalogContext& context) {
            return context.activePlans.empty()
                || std::find(context.activePlans.begin(), context.activePlans.end(), planId)
                    != context.activePlans.end();
        }

        bool scopeActive(const PlanScope& scope, const CatalogContext& context) {
            // A completely empty context retains the query API's historical
            // "all loaded data" behavior. CatalogStore always resolves a
            // persisted selection to at least the General plan, so its
            // contexts take the scoped path below.
            if (context.activePlans.empty()
                && context.countryCode.empty()
                && context.subdivision.empty()
                && context.ituRegionMask == 0) {
                return true;
            }
            if (scope.ituRegionMask != 0
                && (context.ituRegionMask == 0
                    || (scope.ituRegionMask & context.ituRegionMask) == 0)) {
                return false;
            }
            if (!scope.countryCodes.empty()
                && (context.countryCode.empty()
                    || std::find(
                        scope.countryCodes.begin(),
                        scope.countryCodes.end(),
                        context.countryCode) == scope.countryCodes.end())) {
                return false;
            }
            if (!scope.subdivisions.empty()
                && (context.subdivision.empty()
                    || std::find(
                        scope.subdivisions.begin(),
                        scope.subdivisions.end(),
                        context.subdivision) == scope.subdivisions.end())) {
                return false;
            }
            return true;
        }

        bool documentMatchesLayer(
            CatalogLayer layer,
            const CatalogDocument& document,
            std::string& error) {
            if (layer != CatalogLayer::System && layer != CatalogLayer::User) {
                error = "only system and user documents are static catalog layers";
                return false;
            }
            for (size_t i = 0; i < document.bookmarks.size(); i++) {
                if (document.bookmarks[i].layer != layer) {
                    error = "bookmarks[" + std::to_string(i)
                        + "].layer does not match the layer being replaced";
                    return false;
                }
            }
            return true;
        }

        bool validateMergedReferences(
            const CatalogDocument& systemDocument,
            const CatalogDocument& userDocument,
            std::string& error) {
            std::vector<BandPlan> plans = mergeById<BandPlan, PlanId>(
                systemDocument.plans, userDocument.plans,
                [](const BandPlan& plan) -> const PlanId& { return plan.planId; });
            std::vector<Band> bands = mergeById<Band, BandId>(
                systemDocument.bands, userDocument.bands,
                [](const Band& band) -> const BandId& { return band.bandId; });
            std::vector<Segment> segments = mergeById<Segment, SegmentId>(
                systemDocument.segments, userDocument.segments,
                [](const Segment& segment) -> const SegmentId& { return segment.segmentId; });
            std::vector<Bookmark> bookmarks = mergeById<Bookmark, BookmarkId>(
                systemDocument.bookmarks, userDocument.bookmarks,
                [](const Bookmark& bookmark) -> const BookmarkId& { return bookmark.bookmarkId; });

            std::unordered_set<PlanId> planIds;
            std::unordered_set<BandId> bandIds;
            for (const BandPlan& plan : plans) { planIds.insert(plan.planId); }
            for (const Band& band : bands) { bandIds.insert(band.bandId); }
            for (size_t i = 0; i < segments.size(); i++) {
                if (planIds.find(segments[i].planId) == planIds.end()) {
                    error = "merged segments[" + std::to_string(i)
                        + "].plan_id does not resolve to a BandPlan";
                    return false;
                }
                if (segments[i].bandId && bandIds.find(*segments[i].bandId) == bandIds.end()) {
                    error = "merged segments[" + std::to_string(i)
                        + "].band_id does not resolve to a Band";
                    return false;
                }
            }
            for (size_t i = 0; i < bookmarks.size(); i++) {
                if (bookmarks[i].bandId && bandIds.find(*bookmarks[i].bandId) == bandIds.end()) {
                    error = "merged bookmarks[" + std::to_string(i)
                        + "].band_id does not resolve to a Band";
                    return false;
                }
            }
            return true;
        }

        bool validateProviderSnapshot(
            const std::string& provider,
            const ProviderSnapshot& snapshot,
            std::string& error) {
            std::unordered_set<ProviderRecordId> recordIds;
            recordIds.reserve(snapshot.eibiSchedules.size() + snapshot.repeaters.size());
            for (size_t i = 0; i < snapshot.eibiSchedules.size(); i++) {
                const EibiScheduleRecord& record = snapshot.eibiSchedules[i];
                if (record.sourceRef.provider != provider) {
                    error = "eibi_schedules[" + std::to_string(i)
                        + "] belongs to a different provider";
                    return false;
                }
                std::vector<std::string> errors = validate(record);
                if (!errors.empty()) {
                    error = "eibi_schedules[" + std::to_string(i) + "]: " + errors.front();
                    return false;
                }
                if (!recordIds.insert(record.sourceRef.recordId).second) {
                    error = "provider snapshot contains duplicate record_id "
                        + record.sourceRef.recordId.str();
                    return false;
                }
            }
            for (size_t i = 0; i < snapshot.repeaters.size(); i++) {
                const RepeaterRecord& record = snapshot.repeaters[i];
                if (record.sourceRef.provider != provider) {
                    error = "repeaters[" + std::to_string(i)
                        + "] belongs to a different provider";
                    return false;
                }
                std::vector<std::string> errors = validate(record);
                if (!errors.empty()) {
                    error = "repeaters[" + std::to_string(i) + "]: " + errors.front();
                    return false;
                }
                if (!recordIds.insert(record.sourceRef.recordId).second) {
                    error = "provider snapshot contains duplicate record_id "
                        + record.sourceRef.recordId.str();
                    return false;
                }
            }
            return true;
        }
    }

    const BandPlan* CatalogSnapshot::findPlan(const PlanId& id) const {
        auto found = planById.find(id);
        return found == planById.end() ? nullptr : &plans[found->second];
    }

    const Band* CatalogSnapshot::findBand(const BandId& id) const {
        auto found = bandById.find(id);
        return found == bandById.end() ? nullptr : &bands[found->second];
    }

    const Segment* CatalogSnapshot::findSegment(const SegmentId& id) const {
        auto found = segmentById.find(id);
        return found == segmentById.end() ? nullptr : &segments[found->second];
    }

    const Bookmark* CatalogSnapshot::findBookmark(const BookmarkId& id) const {
        auto found = bookmarkById.find(id);
        return found == bookmarkById.end() ? nullptr : &bookmarks[found->second];
    }

    const EibiScheduleRecord* CatalogSnapshot::findEibiRecord(const ProviderRecordId& id) const {
        auto found = eibiById.find(id);
        return found == eibiById.end() ? nullptr : &eibiSchedules[found->second];
    }

    const RepeaterRecord* CatalogSnapshot::findRepeater(const ProviderRecordId& id) const {
        auto found = repeaterById.find(id);
        return found == repeaterById.end() ? nullptr : &repeaters[found->second];
    }

    FrequencyCatalog::FrequencyCatalog() {
        systemDocument.schemaVersion = CATALOG_SCHEMA_VERSION;
        userDocument.schemaVersion = CATALOG_SCHEMA_VERSION;
        publishedSnapshot = buildSnapshotLocked();
    }

    std::shared_ptr<const CatalogSnapshot> FrequencyCatalog::snapshot() const {
        return std::atomic_load_explicit(&publishedSnapshot, std::memory_order_acquire);
    }

    CatalogQueryResult FrequencyCatalog::queryRange(
        double minFrequency,
        double maxFrequency,
        const CatalogContext& context) const {
        if (!std::isfinite(minFrequency) || !std::isfinite(maxFrequency)
            || minFrequency < 0.0 || maxFrequency < minFrequency) {
            throw std::invalid_argument("invalid frequency catalog query range");
        }

        CatalogQueryResult result;
        result.keepAlive = snapshot();
        if (!result.keepAlive) {
            return result;
        }
        const CatalogSnapshot& data = *result.keepAlive;
        std::unordered_set<BandId> matchedBandIds;
        for (const Segment& segment : data.segments) {
            if (planActive(segment.planId, context)
                && rangesOverlap(segment.range, minFrequency, maxFrequency)) {
                result.segments.push_back(&segment);
                if (segment.bandId) {
                    matchedBandIds.insert(*segment.bandId);
                }
            }
        }
        for (const Band& band : data.bands) {
            if (matchedBandIds.find(band.bandId) != matchedBandIds.end()) {
                result.bands.push_back(&band);
            }
        }
        std::vector<const Bookmark*> bookmarkCandidates = queryFrequencyOrder(
            data.bookmarks, data.bookmarkFrequencyOrder, minFrequency, maxFrequency,
            [](const Bookmark& bookmark) { return bookmark.frequency; });
        result.bookmarks.reserve(bookmarkCandidates.size());
        for (const Bookmark* bookmark : bookmarkCandidates) {
            if (scopeActive(bookmark->scope, context)) {
                result.bookmarks.push_back(bookmark);
            }
        }
        result.eibiSchedules = queryFrequencyOrder(
            data.eibiSchedules, data.eibiFrequencyOrder, minFrequency, maxFrequency,
            [](const EibiScheduleRecord& record) { return record.tuningFrequency; });
        result.repeaters = queryFrequencyOrder(
            data.repeaters, data.repeaterFrequencyOrder, minFrequency, maxFrequency,
            [](const RepeaterRecord& record) { return record.outputFrequency; });
        return result;
    }

    bool FrequencyCatalog::replaceLayer(
        CatalogLayer layer,
        CatalogDocument document,
        std::string& error) {
        std::vector<std::string> errors = validate(document);
        if (!errors.empty()) {
            error = errors.front();
            return false;
        }
        if (!documentMatchesLayer(layer, document, error)) {
            return false;
        }
        std::lock_guard<std::mutex> lock(writerMutex);
        const CatalogDocument& nextSystem =
            layer == CatalogLayer::System ? document : systemDocument;
        const CatalogDocument& nextUser =
            layer == CatalogLayer::User ? document : userDocument;
        if (!validateMergedReferences(nextSystem, nextUser, error)) {
            return false;
        }
        if (layer == CatalogLayer::System) {
            systemDocument = std::move(document);
        }
        else {
            userDocument = std::move(document);
        }
        publishRebuiltSnapshotLocked();
        error.clear();
        return true;
    }

    bool FrequencyCatalog::canReplaceLayer(
        CatalogLayer layer,
        const CatalogDocument& document,
        std::string& error) const {
        std::vector<std::string> errors = validate(document);
        if (!errors.empty()) {
            error = errors.front();
            return false;
        }
        if (!documentMatchesLayer(layer, document, error)) {
            return false;
        }
        std::lock_guard<std::mutex> lock(writerMutex);
        const CatalogDocument& nextSystem =
            layer == CatalogLayer::System ? document : systemDocument;
        const CatalogDocument& nextUser =
            layer == CatalogLayer::User ? document : userDocument;
        if (!validateMergedReferences(nextSystem, nextUser, error)) {
            return false;
        }
        error.clear();
        return true;
    }

    bool FrequencyCatalog::clearLayer(CatalogLayer layer, std::string& error) {
        if (layer != CatalogLayer::System && layer != CatalogLayer::User) {
            error = "only system and user documents are static catalog layers";
            return false;
        }
        CatalogDocument empty;
        empty.schemaVersion = CATALOG_SCHEMA_VERSION;
        std::lock_guard<std::mutex> lock(writerMutex);
        const CatalogDocument& nextSystem =
            layer == CatalogLayer::System ? empty : systemDocument;
        const CatalogDocument& nextUser =
            layer == CatalogLayer::User ? empty : userDocument;
        if (!validateMergedReferences(nextSystem, nextUser, error)) {
            return false;
        }
        if (layer == CatalogLayer::System) {
            systemDocument = std::move(empty);
        }
        else {
            userDocument = std::move(empty);
        }
        publishRebuiltSnapshotLocked();
        error.clear();
        return true;
    }

    ProviderRegistration FrequencyCatalog::registerProvider(
        const std::string& provider,
        std::string& error) {
        if (!isValidProviderName(provider)) {
            error = "invalid frequency catalog provider name";
            return {};
        }
        std::lock_guard<std::mutex> lock(writerMutex);
        if (providerStates.find(provider) != providerStates.end()) {
            error = "frequency catalog provider is already registered: " + provider;
            return {};
        }
        ProviderRegistration registration{ provider, nextProviderToken++ };
        if (nextProviderToken == 0) { nextProviderToken = 1; }
        ProviderState state;
        state.token = registration.token;
        providerStates.emplace(provider, std::move(state));
        publishRebuiltSnapshotLocked();
        error.clear();
        return registration;
    }

    bool FrequencyCatalog::publishProviderSnapshot(
        const ProviderRegistration& registration,
        ProviderSnapshot providerSnapshot,
        std::string& error) {
        if (!validateProviderSnapshot(registration.provider, providerSnapshot, error)) {
            return false;
        }
        std::lock_guard<std::mutex> lock(writerMutex);
        if (!checkRegistrationLocked(registration, error)) {
            return false;
        }
        ProviderState& state = providerStates[registration.provider];
        if (state.hasRevision && providerSnapshot.revision <= state.lastRevision) {
            error = "provider snapshot revision must increase";
            return false;
        }
        state.lastRevision = providerSnapshot.revision;
        state.hasRevision = true;
        state.snapshot = std::move(providerSnapshot);
        state.hasSnapshot = true;
        publishRebuiltSnapshotLocked();
        error.clear();
        return true;
    }

    bool FrequencyCatalog::clearProviderSnapshot(
        const ProviderRegistration& registration,
        uint64_t revision,
        std::string& error) {
        std::lock_guard<std::mutex> lock(writerMutex);
        if (!checkRegistrationLocked(registration, error)) {
            return false;
        }
        ProviderState& state = providerStates[registration.provider];
        if (state.hasRevision && revision <= state.lastRevision) {
            error = "provider clear revision must increase";
            return false;
        }
        state.lastRevision = revision;
        state.hasRevision = true;
        state.snapshot = {};
        state.hasSnapshot = false;
        publishRebuiltSnapshotLocked();
        error.clear();
        return true;
    }

    bool FrequencyCatalog::unregisterProvider(const ProviderRegistration& registration) {
        std::lock_guard<std::mutex> lock(writerMutex);
        auto found = providerStates.find(registration.provider);
        if (found == providerStates.end() || found->second.token != registration.token) {
            return false;
        }
        providerStates.erase(found);
        publishRebuiltSnapshotLocked();
        return true;
    }

    std::shared_ptr<const CatalogSnapshot> FrequencyCatalog::buildSnapshotLocked() const {
        auto result = std::make_shared<CatalogSnapshot>();
        result->generation = nextGeneration;
        result->plans = mergeById<BandPlan, PlanId>(
            systemDocument.plans, userDocument.plans,
            [](const BandPlan& plan) -> const PlanId& { return plan.planId; });
        result->bands = mergeById<Band, BandId>(
            systemDocument.bands, userDocument.bands,
            [](const Band& band) -> const BandId& { return band.bandId; });
        result->segments = mergeById<Segment, SegmentId>(
            systemDocument.segments, userDocument.segments,
            [](const Segment& segment) -> const SegmentId& { return segment.segmentId; });
        result->bookmarks = mergeById<Bookmark, BookmarkId>(
            systemDocument.bookmarks, userDocument.bookmarks,
            [](const Bookmark& bookmark) -> const BookmarkId& { return bookmark.bookmarkId; });

        for (const auto& [provider, state] : providerStates) {
            result->providers.push_back(provider);
            if (!state.hasSnapshot) { continue; }
            result->eibiSchedules.insert(result->eibiSchedules.end(),
                state.snapshot.eibiSchedules.begin(), state.snapshot.eibiSchedules.end());
            result->repeaters.insert(result->repeaters.end(),
                state.snapshot.repeaters.begin(), state.snapshot.repeaters.end());
        }
        for (size_t i = 0; i < result->plans.size(); i++) {
            result->planById[result->plans[i].planId] = i;
        }
        for (size_t i = 0; i < result->bands.size(); i++) {
            result->bandById[result->bands[i].bandId] = i;
        }
        for (size_t i = 0; i < result->segments.size(); i++) {
            result->segmentById[result->segments[i].segmentId] = i;
        }
        for (size_t i = 0; i < result->bookmarks.size(); i++) {
            result->bookmarkById[result->bookmarks[i].bookmarkId] = i;
        }
        for (size_t i = 0; i < result->eibiSchedules.size(); i++) {
            result->eibiById[result->eibiSchedules[i].sourceRef.recordId] = i;
        }
        for (size_t i = 0; i < result->repeaters.size(); i++) {
            result->repeaterById[result->repeaters[i].sourceRef.recordId] = i;
        }
        sortFrequencyOrder(result->bookmarks, result->bookmarkFrequencyOrder,
            [](const Bookmark& value) { return value.frequency; });
        sortFrequencyOrder(result->eibiSchedules, result->eibiFrequencyOrder,
            [](const EibiScheduleRecord& value) { return value.tuningFrequency; });
        sortFrequencyOrder(result->repeaters, result->repeaterFrequencyOrder,
            [](const RepeaterRecord& value) { return value.outputFrequency; });
        return result;
    }

    void FrequencyCatalog::publishRebuiltSnapshotLocked() {
        ++nextGeneration;
        std::shared_ptr<const CatalogSnapshot> next = buildSnapshotLocked();
        std::atomic_store_explicit(&publishedSnapshot, std::move(next), std::memory_order_release);
    }

    bool FrequencyCatalog::checkRegistrationLocked(
        const ProviderRegistration& registration,
        std::string& error) const {
        auto found = providerStates.find(registration.provider);
        if (found == providerStates.end() || found->second.token != registration.token) {
            error = "frequency catalog provider registration is stale";
            return false;
        }
        return true;
    }

}

namespace core {
    frequency_catalog::FrequencyCatalog& getFrequencyCatalog() {
        static frequency_catalog::FrequencyCatalog catalog;
        return catalog;
    }
}
