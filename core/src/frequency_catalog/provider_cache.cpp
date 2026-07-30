#include "provider_cache.h"

#include <exception>
#include <fstream>
#include <system_error>
#include <unordered_set>
#include <utility>

#ifdef _WIN32
#include <Windows.h>
#endif

namespace frequency_catalog {

    namespace {
        constexpr uintmax_t MEBIBYTE = 1024U * 1024U;
        constexpr uintmax_t EIBI_MAX_CACHE_FILE_SIZE = 16U * MEBIBYTE;
        constexpr uintmax_t REPEATERBOOK_MAX_CACHE_FILE_SIZE = 32U * MEBIBYTE;
        constexpr uintmax_t DEFAULT_MAX_CACHE_FILE_SIZE = 64U * MEBIBYTE;
        constexpr size_t MAX_EIBI_RECORDS = 25000;
        constexpr size_t MAX_REPEATER_RECORDS = 50000;

        uintmax_t maxCacheFileSize(const std::string& provider) {
            if (provider == "eibi") {
                return EIBI_MAX_CACHE_FILE_SIZE;
            }
            if (provider == "repeaterbook") {
                return REPEATERBOOK_MAX_CACHE_FILE_SIZE;
            }
            return DEFAULT_MAX_CACHE_FILE_SIZE;
        }

        std::string cacheSizeError(
            const std::string& provider,
            uintmax_t limit) {
            return "provider cache for " + provider + " exceeds the "
                + std::to_string(limit / MEBIBYTE) + " MiB size limit";
        }

        size_t snapshotRecordCount(const ProviderSnapshot& snapshot) {
            return snapshot.eibiSchedules.size() + snapshot.repeaters.size();
        }

        bool validateSnapshot(
            const std::string& provider,
            const ProviderSnapshot& snapshot,
            std::string& error) {
            if (snapshot.eibiSchedules.size() > MAX_EIBI_RECORDS) {
                error = "provider snapshot exceeds the 25000-record EiBi limit";
                return false;
            }
            if (snapshot.repeaters.size() > MAX_REPEATER_RECORDS) {
                error = "provider snapshot exceeds the 50000-record repeater limit";
                return false;
            }
            std::unordered_set<ProviderRecordId> ids;
            ids.reserve(snapshotRecordCount(snapshot));
            for (size_t i = 0; i < snapshot.eibiSchedules.size(); i++) {
                const EibiScheduleRecord& record = snapshot.eibiSchedules[i];
                if (record.sourceRef.provider != provider) {
                    error = "eibi_schedules[" + std::to_string(i)
                        + "] belongs to another provider";
                    return false;
                }
                std::vector<std::string> errors = validate(record);
                if (!errors.empty()) {
                    error = "eibi_schedules[" + std::to_string(i) + "]: " + errors.front();
                    return false;
                }
                if (!ids.insert(record.sourceRef.recordId).second) {
                    error = "duplicate provider record_id: " + record.sourceRef.recordId.str();
                    return false;
                }
            }
            for (size_t i = 0; i < snapshot.repeaters.size(); i++) {
                const RepeaterRecord& record = snapshot.repeaters[i];
                if (record.sourceRef.provider != provider) {
                    error = "repeaters[" + std::to_string(i)
                        + "] belongs to another provider";
                    return false;
                }
                std::vector<std::string> errors = validate(record);
                if (!errors.empty()) {
                    error = "repeaters[" + std::to_string(i) + "]: " + errors.front();
                    return false;
                }
                if (!ids.insert(record.sourceRef.recordId).second) {
                    error = "duplicate provider record_id: " + record.sourceRef.recordId.str();
                    return false;
                }
            }
            return true;
        }

        bool replaceFile(
            const std::filesystem::path& temporary,
            const std::filesystem::path& target,
            std::string& error) {
#ifdef _WIN32
            if (!MoveFileExW(
                    temporary.c_str(),
                    target.c_str(),
                    MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
                error = "could not replace provider cache file (Windows error "
                    + std::to_string(GetLastError()) + ")";
                return false;
            }
            return true;
#else
            std::error_code ec;
            std::filesystem::rename(temporary, target, ec);
            if (ec) {
                error = "could not replace provider cache file: " + ec.message();
                return false;
            }
            return true;
#endif
        }

        ProviderCacheLoadResult invalidResult(std::string error) {
            ProviderCacheLoadResult result;
            result.status = ProviderCacheStatus::Invalid;
            result.error = std::move(error);
            return result;
        }
    }

    ProviderCacheStore::ProviderCacheStore(std::filesystem::path directory)
        : directory(std::move(directory)) {
    }

    std::filesystem::path ProviderCacheStore::pathFor(const std::string& provider) const {
        if (!isValidProviderName(provider)) {
            return {};
        }
        return directory / (provider + ".json");
    }

    ProviderCacheLoadResult ProviderCacheStore::load(
        const std::string& provider,
        const std::string& expectedScopeKey,
        int64_t nowUnix) const {
        if (!isValidProviderName(provider)) {
            return invalidResult("invalid provider cache name");
        }
        std::filesystem::path path = pathFor(provider);
        std::error_code ec;
        if (!std::filesystem::exists(path, ec)) {
            ProviderCacheLoadResult result;
            result.status = ProviderCacheStatus::Missing;
            if (ec) {
                result.status = ProviderCacheStatus::Invalid;
                result.error = "could not inspect provider cache: " + ec.message();
            }
            return result;
        }
        if (!std::filesystem::is_regular_file(path, ec) || ec) {
            return invalidResult("provider cache path is not a regular file");
        }
        uintmax_t fileSize = std::filesystem::file_size(path, ec);
        uintmax_t fileSizeLimit = maxCacheFileSize(provider);
        if (ec || fileSize > fileSizeLimit) {
            return invalidResult(ec
                ? "could not inspect provider cache size"
                : cacheSizeError(provider, fileSizeLimit));
        }

        try {
            std::ifstream input(path, std::ios::binary);
            if (!input) {
                return invalidResult("could not open provider cache");
            }
            json document;
            input >> document;
            CachedProviderSnapshot cache = document.get<CachedProviderSnapshot>();
            if (cache.manifest.schemaVersion != PROVIDER_CACHE_SCHEMA_VERSION) {
                return invalidResult("provider cache schema is unsupported");
            }
            if (cache.manifest.provider != provider) {
                return invalidResult("provider cache contains a different provider");
            }
            if (cache.manifest.revision == 0
                || cache.snapshot.revision != cache.manifest.revision) {
                return invalidResult("provider cache revision is inconsistent");
            }
            if (cache.manifest.recordCount != snapshotRecordCount(cache.snapshot)) {
                return invalidResult("provider cache record count is inconsistent");
            }
            if (!expectedScopeKey.empty()
                && cache.manifest.scopeKey != expectedScopeKey) {
                return invalidResult("provider cache scope does not match");
            }
            std::string error;
            if (!validateSnapshot(provider, cache.snapshot, error)) {
                return invalidResult(std::move(error));
            }
            ProviderCacheLoadResult result;
            result.status = cache.manifest.isFreshAt(nowUnix)
                ? ProviderCacheStatus::Fresh
                : ProviderCacheStatus::Stale;
            result.cache = std::move(cache);
            return result;
        }
        catch (const std::exception& e) {
            return invalidResult(std::string("could not parse provider cache: ") + e.what());
        }
    }

    bool ProviderCacheStore::store(
        const CachedProviderSnapshot& cache,
        std::string& error) const {
        if (!isValidProviderName(cache.manifest.provider)) {
            error = "invalid provider cache name";
            return false;
        }
        if (cache.manifest.schemaVersion != PROVIDER_CACHE_SCHEMA_VERSION) {
            error = "provider cache schema is unsupported";
            return false;
        }
        if (cache.manifest.revision == 0
            || cache.snapshot.revision != cache.manifest.revision) {
            error = "provider cache revision is inconsistent";
            return false;
        }
        if (cache.manifest.fetchedAtUnix <= 0
            || cache.manifest.expiresAtUnix <= cache.manifest.fetchedAtUnix) {
            error = "provider cache freshness interval is invalid";
            return false;
        }
        if (!validateSnapshot(cache.manifest.provider, cache.snapshot, error)) {
            return false;
        }
        CachedProviderSnapshot persisted = cache;
        persisted.manifest.recordCount = snapshotRecordCount(persisted.snapshot);

        std::error_code ec;
        std::filesystem::create_directories(directory, ec);
        if (ec) {
            error = "could not create provider cache directory: " + ec.message();
            return false;
        }
        std::filesystem::path target = pathFor(cache.manifest.provider);
        std::filesystem::path temporary = target;
        temporary += ".tmp-" + std::to_string(cache.manifest.revision);
        {
            std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
            if (!output) {
                error = "could not open temporary provider cache";
                return false;
            }
            output << json(persisted).dump();
            output.flush();
            if (!output) {
                output.close();
                std::filesystem::remove(temporary, ec);
                error = "could not write temporary provider cache";
                return false;
            }
        }
        uintmax_t fileSize = std::filesystem::file_size(temporary, ec);
        uintmax_t fileSizeLimit = maxCacheFileSize(cache.manifest.provider);
        if (ec || fileSize > fileSizeLimit) {
            std::error_code cleanupError;
            std::filesystem::remove(temporary, cleanupError);
            error = ec
                ? "could not inspect temporary provider cache size"
                : cacheSizeError(cache.manifest.provider, fileSizeLimit);
            return false;
        }
        if (!replaceFile(temporary, target, error)) {
            std::filesystem::remove(temporary, ec);
            return false;
        }
        error.clear();
        return true;
    }

    void to_json(json& j, const ProviderCacheManifest& value) {
        j = {
            { "schema_version", value.schemaVersion },
            { "provider", value.provider },
            { "revision", value.revision },
            { "fetched_at_unix", value.fetchedAtUnix },
            { "expires_at_unix", value.expiresAtUnix },
            { "record_count", value.recordCount }
        };
        if (!value.sourceUrl.empty()) { j["source_url"] = value.sourceUrl; }
        if (!value.etag.empty()) { j["etag"] = value.etag; }
        if (!value.lastModified.empty()) { j["last_modified"] = value.lastModified; }
        if (!value.scopeKey.empty()) { j["scope_key"] = value.scopeKey; }
    }

    void from_json(const json& j, ProviderCacheManifest& value) {
        j.at("schema_version").get_to(value.schemaVersion);
        j.at("provider").get_to(value.provider);
        j.at("revision").get_to(value.revision);
        j.at("fetched_at_unix").get_to(value.fetchedAtUnix);
        j.at("expires_at_unix").get_to(value.expiresAtUnix);
        j.at("record_count").get_to(value.recordCount);
        value.sourceUrl = j.value("source_url", "");
        value.etag = j.value("etag", "");
        value.lastModified = j.value("last_modified", "");
        value.scopeKey = j.value("scope_key", "");
    }

    void to_json(json& j, const ProviderSnapshot& value) {
        j = {
            { "revision", value.revision },
            { "eibi_schedules", value.eibiSchedules },
            { "repeaters", value.repeaters }
        };
    }

    void from_json(const json& j, ProviderSnapshot& value) {
        j.at("revision").get_to(value.revision);
        value.eibiSchedules =
            j.value("eibi_schedules", std::vector<EibiScheduleRecord>{});
        value.repeaters =
            j.value("repeaters", std::vector<RepeaterRecord>{});
    }

    void to_json(json& j, const CachedProviderSnapshot& value) {
        j = {
            { "manifest", value.manifest },
            { "snapshot", value.snapshot }
        };
    }

    void from_json(const json& j, CachedProviderSnapshot& value) {
        j.at("manifest").get_to(value.manifest);
        j.at("snapshot").get_to(value.snapshot);
    }

}
