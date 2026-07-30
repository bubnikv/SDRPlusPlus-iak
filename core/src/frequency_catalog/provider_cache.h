#pragma once

#include "catalog.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>

namespace frequency_catalog {

    inline constexpr int PROVIDER_CACHE_SCHEMA_VERSION = 1;

    // Cache identity and HTTP refresh metadata are intentionally versioned
    // separately from CatalogDocument. scopeKey is provider-defined: an EIBI
    // season (for example "a26") or a rounded receiver-location key for a
    // proximity-based repeater query.
    struct ProviderCacheManifest {
        int schemaVersion = PROVIDER_CACHE_SCHEMA_VERSION;
        std::string provider;
        uint64_t revision = 0;
        int64_t fetchedAtUnix = 0;
        int64_t expiresAtUnix = 0;
        std::string sourceUrl;
        std::string etag;
        std::string lastModified;
        std::string scopeKey;
        size_t recordCount = 0;

        bool isFreshAt(int64_t unixTime) const {
            return expiresAtUnix > 0 && unixTime < expiresAtUnix;
        }
    };

    struct CachedProviderSnapshot {
        ProviderCacheManifest manifest;
        ProviderSnapshot snapshot;
    };

    enum class ProviderCacheStatus {
        Missing,
        Fresh,
        Stale,
        Invalid
    };

    struct ProviderCacheLoadResult {
        ProviderCacheStatus status = ProviderCacheStatus::Missing;
        CachedProviderSnapshot cache;
        std::string error;

        bool hasSnapshot() const {
            return status == ProviderCacheStatus::Fresh
                || status == ProviderCacheStatus::Stale;
        }
    };

    // One self-contained JSON file per provider. A single file avoids manifest
    // and payload skew after Android process eviction. Writes use a sibling
    // temporary file followed by an OS-level replacement.
    class ProviderCacheStore {
    public:
        explicit ProviderCacheStore(std::filesystem::path directory);

        ProviderCacheLoadResult load(
            const std::string& provider,
            const std::string& expectedScopeKey,
            int64_t nowUnix) const;

        bool store(
            const CachedProviderSnapshot& cache,
            std::string& error) const;

        std::filesystem::path pathFor(const std::string& provider) const;

    private:
        std::filesystem::path directory;
    };

    void to_json(json& j, const ProviderCacheManifest& value);
    void from_json(const json& j, ProviderCacheManifest& value);
    void to_json(json& j, const ProviderSnapshot& value);
    void from_json(const json& j, ProviderSnapshot& value);
    void to_json(json& j, const CachedProviderSnapshot& value);
    void from_json(const json& j, CachedProviderSnapshot& value);

}
