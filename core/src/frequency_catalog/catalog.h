#pragma once

#include "schema.h"

#include <module.h>

#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace frequency_catalog {

    class CatalogStore;

    // A provider registration is a capability token. The token prevents a
    // stopped/unloaded module from clearing or publishing over a newer module
    // that subsequently registered the same provider name.
    struct ProviderRegistration {
        std::string provider;
        uint64_t token = 0;

        explicit operator bool() const {
            return !provider.empty() && token != 0;
        }
    };

    // Dynamic provider data after download/cache parsing. revision must
    // increase for each publication by one provider registration.
    struct ProviderSnapshot {
        uint64_t revision = 0;
        std::vector<EibiScheduleRecord> eibiSchedules;
        std::vector<RepeaterRecord> repeaters;
    };

    // Immutable after publication. Callers receive shared_ptr<const ...>;
    // pointers returned by lookup methods remain valid while that shared_ptr is
    // retained.
    struct CatalogSnapshot {
        uint64_t generation = 0;
        std::vector<std::string> providers;
        std::vector<BandPlan> plans;
        std::vector<Band> bands;
        std::vector<Segment> segments;
        std::vector<Bookmark> bookmarks;
        std::vector<EibiScheduleRecord> eibiSchedules;
        std::vector<RepeaterRecord> repeaters;

        const BandPlan* findPlan(const PlanId& id) const;
        const Band* findBand(const BandId& id) const;
        const Segment* findSegment(const SegmentId& id) const;
        const Bookmark* findBookmark(const BookmarkId& id) const;
        const EibiScheduleRecord* findEibiRecord(const ProviderRecordId& id) const;
        const RepeaterRecord* findRepeater(const ProviderRecordId& id) const;

    private:
        friend class FrequencyCatalog;

        std::unordered_map<PlanId, size_t> planById;
        std::unordered_map<BandId, size_t> bandById;
        std::unordered_map<SegmentId, size_t> segmentById;
        std::unordered_map<BookmarkId, size_t> bookmarkById;
        std::unordered_map<ProviderRecordId, size_t> eibiById;
        std::unordered_map<ProviderRecordId, size_t> repeaterById;

        std::vector<size_t> bookmarkFrequencyOrder;
        std::vector<size_t> eibiFrequencyOrder;
        std::vector<size_t> repeaterFrequencyOrder;
    };

    // Empty activePlans means "all loaded plans". Supplying plans and/or
    // geographic fields is the caller's explicit regulatory context; the
    // catalog never chooses a region by frequency or insertion order.
    struct CatalogContext {
        std::vector<PlanId> activePlans;
        std::string countryCode;
        std::string subdivision;
        uint8_t ituRegionMask = 0;
    };

    // Result pointers are owned by keepAlive. Keeping this result object alive
    // is sufficient even if a provider publishes a replacement concurrently.
    struct CatalogQueryResult {
        std::shared_ptr<const CatalogSnapshot> keepAlive;
        std::vector<const Band*> bands;
        std::vector<const Segment*> segments;
        std::vector<const Bookmark*> bookmarks;
        std::vector<const EibiScheduleRecord*> eibiSchedules;
        std::vector<const RepeaterRecord*> repeaters;

        uint64_t generation() const {
            return keepAlive ? keepAlive->generation : 0;
        }
    };

    class FrequencyCatalog {
    public:
        FrequencyCatalog();

        FrequencyCatalog(const FrequencyCatalog&) = delete;
        FrequencyCatalog& operator=(const FrequencyCatalog&) = delete;

        // Reader operations use atomic shared_ptr publication and do not take
        // the writer mutex. queryRange returns frequency candidates only;
        // schedule, UTC and distance filtering belongs to the provider/query
        // projection layer added later.
        std::shared_ptr<const CatalogSnapshot> snapshot() const;
        CatalogQueryResult queryRange(
            double minFrequency,
            double maxFrequency,
            const CatalogContext& context = {}) const;

        ProviderRegistration registerProvider(
            const std::string& provider,
            std::string& error);
        bool publishProviderSnapshot(
            const ProviderRegistration& registration,
            ProviderSnapshot providerSnapshot,
            std::string& error);
        bool clearProviderSnapshot(
            const ProviderRegistration& registration,
            uint64_t revision,
            std::string& error);
        bool unregisterProvider(
            const ProviderRegistration& registration);

    private:
        friend class CatalogStore;

        struct ProviderState {
            uint64_t token = 0;
            bool hasSnapshot = false;
            bool hasRevision = false;
            uint64_t lastRevision = 0;
            ProviderSnapshot snapshot;
        };

        std::shared_ptr<const CatalogSnapshot> buildSnapshotLocked() const;
        void publishRebuiltSnapshotLocked();
        bool checkRegistrationLocked(
            const ProviderRegistration& registration,
            std::string& error) const;
        bool canReplaceLayer(
            CatalogLayer layer,
            const CatalogDocument& document,
            std::string& error) const;
        bool replaceLayer(
            CatalogLayer layer,
            CatalogDocument document,
            std::string& error);
        bool clearLayer(CatalogLayer layer, std::string& error);

        mutable std::mutex writerMutex;
        CatalogDocument systemDocument;
        CatalogDocument userDocument;
        std::map<std::string, ProviderState> providerStates;
        uint64_t nextProviderToken = 1;
        uint64_t nextGeneration = 0;
        std::shared_ptr<const CatalogSnapshot> publishedSnapshot;
    };

}
