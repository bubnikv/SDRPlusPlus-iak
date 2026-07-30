#pragma once

#include "catalog.h"

#include <filesystem>
#include <mutex>
#include <string>
#include <vector>

namespace frequency_catalog {

    inline constexpr int CATALOG_CONTEXT_SCHEMA_VERSION = 1;

    struct CatalogSupplementalSourceInfo {
        std::string provider;
        std::string name;
        std::string title;
        std::string revision;
        std::string url;
        std::string checkedOn;
    };

    struct CatalogSourceInfo {
        std::string name;
        std::string revision;
        std::string attribution;
        std::string license;
        std::string noticePath;
        std::string licensePath;
        std::vector<CatalogSupplementalSourceInfo> supplements;
    };

    // Persisted independently from UI/module configuration so the same
    // regulatory context is available to every catalog consumer.
    struct CatalogContextSelection {
        std::vector<PlanId> explicitPlanIds;
        std::string countryCode;
        std::string subdivision;
        int ituRegion = 0; // 0 = unspecified, otherwise 1..3
    };

    class CatalogStore {
    public:
        explicit CatalogStore(FrequencyCatalog& catalog);

        CatalogStore(const CatalogStore&) = delete;
        CatalogStore& operator=(const CatalogStore&) = delete;

        // Loads the immutable packaged system layer and the mutable user layer.
        // resourcesDirectory is the application's distribution resource root;
        // applicationDataDirectory is a writable, app-private directory.
        bool initialize(
            const std::filesystem::path& resourcesDirectory,
            const std::filesystem::path& applicationDataDirectory,
            std::string& error);

        bool initialized() const;
        CatalogSourceInfo sourceInfo() const;
        CatalogDocument userDocument() const;
        CatalogContextSelection contextSelection() const;
        CatalogContext activeContext() const;
        std::vector<std::string> recoveryWarnings() const;

        // User-layer mutations are intentionally bookmark-scoped. System data
        // can only be installed by this core store.
        bool upsertUserBookmark(Bookmark bookmark, std::string& error);
        bool removeUserBookmark(const BookmarkId& bookmarkId, std::string& error);
        bool replaceUserBookmarks(
            std::vector<Bookmark> bookmarks,
            std::string& error);

        bool setContextSelection(
            CatalogContextSelection selection,
            std::string& error);

    private:
        bool commitUserDocument(CatalogDocument document, std::string& error);
        CatalogContext resolveContextLocked(
            const CatalogContextSelection& selection) const;

        FrequencyCatalog& catalog;
        mutable std::mutex mutex;
        std::filesystem::path dataDirectory;
        CatalogDocument loadedUserDocument;
        CatalogSourceInfo loadedSourceInfo;
        CatalogContextSelection loadedSelection;
        CatalogContext resolvedContext;
        std::vector<std::string> warnings;
        bool isInitialized = false;
    };

    void to_json(json& j, const CatalogContextSelection& value);
    void from_json(const json& j, CatalogContextSelection& value);

}
