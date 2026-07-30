#include "store.h"

#include <core.h>
#include <utils/proto/picohash.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <exception>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <system_error>
#include <unordered_set>
#include <utility>

#ifdef _WIN32
#include <Windows.h>
#endif

namespace frequency_catalog {

    namespace {
        constexpr uintmax_t MEBIBYTE = 1024U * 1024U;
        constexpr uintmax_t MAX_SYSTEM_CATALOG_SIZE = 8U * MEBIBYTE;
        constexpr uintmax_t MAX_USER_CATALOG_SIZE = 8U * MEBIBYTE;
        constexpr uintmax_t MAX_MANIFEST_SIZE = 64U * 1024U;
        constexpr uintmax_t MAX_CONTEXT_SIZE = 64U * 1024U;
        constexpr size_t MAX_USER_BOOKMARKS = 50000;
        constexpr size_t MAX_SUPPLEMENTAL_SOURCES = 16;
        constexpr int RUNTIME_MANIFEST_SCHEMA_VERSION = 1;

        struct RuntimeCatalogEntry {
            std::string encoding;
            std::filesystem::path path;
            std::string sha256;
            uintmax_t size = 0;
            int wireSchemaVersion = 0;
        };

        struct RuntimeManifest {
            RuntimeCatalogEntry catalog;
            CatalogSourceInfo source;
            int catalogSchemaVersion = 0;
            size_t plans = 0;
            size_t bands = 0;
            size_t segments = 0;
            size_t bookmarks = 0;
        };

        bool readBoundedFile(
            const std::filesystem::path& path,
            uintmax_t limit,
            std::vector<uint8_t>& bytes,
            std::string& error) {
            std::error_code ec;
            if (!std::filesystem::is_regular_file(path, ec) || ec) {
                error = "file is missing or is not a regular file: " + path.string();
                return false;
            }
            uintmax_t size = std::filesystem::file_size(path, ec);
            if (ec) {
                error = "could not inspect file size: " + path.string();
                return false;
            }
            if (size > limit) {
                error = "file exceeds the " + std::to_string(limit / 1024U)
                    + " KiB defensive size limit: " + path.string();
                return false;
            }
            std::ifstream input(path, std::ios::binary);
            if (!input) {
                error = "could not open file: " + path.string();
                return false;
            }
            bytes.resize(static_cast<size_t>(size));
            if (size != 0) {
                input.read(
                    reinterpret_cast<char*>(bytes.data()),
                    static_cast<std::streamsize>(size));
            }
            if (!input || input.gcount() != static_cast<std::streamsize>(size)) {
                error = "could not read complete file: " + path.string();
                bytes.clear();
                return false;
            }
            error.clear();
            return true;
        }

        std::string sha256(const std::vector<uint8_t>& bytes) {
            std::array<uint8_t, PICOHASH_SHA256_DIGEST_LENGTH> digest{};
            picohash_ctx_t context;
            picohash_init_sha256(&context);
            if (!bytes.empty()) {
                picohash_update(&context, bytes.data(), bytes.size());
            }
            picohash_final(&context, digest.data());
            std::ostringstream value;
            value << std::hex << std::setfill('0');
            for (uint8_t byte : digest) {
                value << std::setw(2) << static_cast<unsigned int>(byte);
            }
            return value.str();
        }

        bool safeResourceName(const std::filesystem::path& path) {
            return !path.empty()
                && !path.is_absolute()
                && path == path.filename()
                && path != "."
                && path != "..";
        }

        RuntimeManifest parseRuntimeManifest(const json& document) {
            if (!document.is_object()
                || document.at("schema_version").get<int>()
                    != RUNTIME_MANIFEST_SCHEMA_VERSION) {
                throw std::invalid_argument(
                    "unsupported frequency-catalog runtime manifest schema");
            }
            RuntimeManifest manifest;
            manifest.catalogSchemaVersion =
                document.at("catalog_schema_version").get<int>();
            if (manifest.catalogSchemaVersion != CATALOG_SCHEMA_VERSION) {
                throw std::invalid_argument(
                    "packaged catalog schema does not match this application");
            }
#ifdef __ANDROID__
            const char* platform = "android";
            const char* expectedEncoding = "cbor";
#else
            const char* platform = "desktop";
            const char* expectedEncoding = "json";
#endif
            const json& entry = document.at("catalogs").at(platform);
            manifest.catalog.encoding = entry.at("encoding").get<std::string>();
            manifest.catalog.path = entry.at("path").get<std::string>();
            manifest.catalog.sha256 = entry.at("sha256").get<std::string>();
            manifest.catalog.size = entry.at("size").get<uintmax_t>();
            manifest.catalog.wireSchemaVersion =
                entry.value("wire_schema_version", 0);
            if (manifest.catalog.encoding != expectedEncoding) {
                throw std::invalid_argument(
                    "packaged catalog encoding does not match this platform");
            }
            if (!safeResourceName(manifest.catalog.path)) {
                throw std::invalid_argument(
                    "packaged catalog path is not a safe resource filename");
            }
            if (manifest.catalog.size == 0
                || manifest.catalog.size > MAX_SYSTEM_CATALOG_SIZE) {
                throw std::invalid_argument(
                    "packaged catalog size is outside the supported bounds");
            }
            if (manifest.catalog.sha256.size() != 64
                || !std::all_of(
                    manifest.catalog.sha256.begin(),
                    manifest.catalog.sha256.end(),
                    [](unsigned char ch) { return std::isxdigit(ch) != 0; })) {
                throw std::invalid_argument(
                    "packaged catalog checksum is malformed");
            }
#ifdef __ANDROID__
            if (manifest.catalog.wireSchemaVersion
                != CATALOG_CBOR_WIRE_SCHEMA_VERSION) {
                throw std::invalid_argument(
                    "packaged CBOR wire schema is unsupported");
            }
#endif
            const json& counts = document.at("counts");
            manifest.plans = counts.at("plans").get<size_t>();
            manifest.bands = counts.at("bands").get<size_t>();
            manifest.segments = counts.at("segments").get<size_t>();
            manifest.bookmarks = counts.at("bookmarks").get<size_t>();

            const json& source = document.at("source");
            manifest.source.name = source.at("name").get<std::string>();
            manifest.source.revision = source.at("revision").get<std::string>();
            manifest.source.attribution =
                source.at("attribution").get<std::string>();
            manifest.source.license = source.at("license").get<std::string>();
            manifest.source.noticePath =
                document.at("notice_path").get<std::string>();
            manifest.source.licensePath =
                document.at("license_path").get<std::string>();
            if (document.contains("supplemental_sources")) {
                const json& supplements = document.at("supplemental_sources");
                if (!supplements.is_array()
                    || supplements.size() > MAX_SUPPLEMENTAL_SOURCES) {
                    throw std::invalid_argument(
                        "packaged catalog has invalid supplemental sources");
                }
                for (const json& item : supplements) {
                    CatalogSupplementalSourceInfo supplement;
                    supplement.provider =
                        item.at("provider").get<std::string>();
                    supplement.name = item.at("name").get<std::string>();
                    supplement.title = item.at("title").get<std::string>();
                    supplement.revision =
                        item.at("revision").get<std::string>();
                    supplement.url = item.at("url").get<std::string>();
                    supplement.checkedOn =
                        item.at("checked_on").get<std::string>();
                    if (supplement.provider.empty()
                        || supplement.name.empty()
                        || supplement.title.empty()
                        || supplement.revision.empty()
                        || supplement.url.empty()
                        || supplement.checkedOn.empty()) {
                        throw std::invalid_argument(
                            "packaged catalog has an incomplete supplemental source");
                    }
                    manifest.source.supplements.push_back(
                        std::move(supplement));
                }
            }
            return manifest;
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
                error = "could not replace catalog file (Windows error "
                    + std::to_string(GetLastError()) + ")";
                return false;
            }
            return true;
#else
            std::error_code ec;
            std::filesystem::rename(temporary, target, ec);
            if (ec) {
                error = "could not replace catalog file: " + ec.message();
                return false;
            }
            return true;
#endif
        }

        bool writeAtomic(
            const std::filesystem::path& target,
            const std::string& contents,
            uintmax_t limit,
            std::string& error) {
            if (contents.size() > limit) {
                error = "serialized catalog state exceeds its defensive size limit";
                return false;
            }
            std::error_code ec;
            std::filesystem::create_directories(target.parent_path(), ec);
            if (ec) {
                error = "could not create catalog data directory: " + ec.message();
                return false;
            }
            std::filesystem::path temporary = target;
            temporary += ".tmp";
            {
                std::ofstream output(
                    temporary,
                    std::ios::binary | std::ios::trunc);
                if (!output) {
                    error = "could not open temporary catalog file";
                    return false;
                }
                output.write(
                    contents.data(),
                    static_cast<std::streamsize>(contents.size()));
                output.flush();
                if (!output) {
                    output.close();
                    std::filesystem::remove(temporary, ec);
                    error = "could not write temporary catalog file";
                    return false;
                }
            }
            if (!replaceFile(temporary, target, error)) {
                std::filesystem::remove(temporary, ec);
                return false;
            }
            error.clear();
            return true;
        }

        bool isEmptyScope(const PlanScope& scope) {
            return scope.ituRegionMask == 0
                && scope.countryCodes.empty()
                && scope.subdivisions.empty();
        }

        std::string uppercaseAscii(std::string value) {
            std::transform(
                value.begin(),
                value.end(),
                value.begin(),
                [](unsigned char ch) {
                    return static_cast<char>(std::toupper(ch));
                });
            return value;
        }

        bool normalizeSelection(
            CatalogContextSelection& selection,
            std::string& error) {
            if (selection.ituRegion < 0 || selection.ituRegion > 3) {
                error = "ITU region must be 0, 1, 2, or 3";
                return false;
            }
            selection.countryCode = uppercaseAscii(selection.countryCode);
            selection.subdivision = uppercaseAscii(selection.subdivision);
            if (!selection.countryCode.empty()
                && (selection.countryCode.size() != 2
                    || !std::all_of(
                        selection.countryCode.begin(),
                        selection.countryCode.end(),
                        [](unsigned char ch) { return std::isalpha(ch) != 0; }))) {
                error = "country code must be a two-letter ISO code";
                return false;
            }
            if (selection.subdivision.size() > 32
                || !std::all_of(
                    selection.subdivision.begin(),
                    selection.subdivision.end(),
                    [](unsigned char ch) {
                        return std::isalnum(ch) != 0 || ch == '-';
                    })) {
                error = "subdivision must contain only letters, digits, or '-'";
                return false;
            }
            std::vector<PlanId> unique;
            std::unordered_set<PlanId> seen;
            unique.reserve(selection.explicitPlanIds.size());
            for (const PlanId& planId : selection.explicitPlanIds) {
                if (!isValidStableId(planId.str())) {
                    error = "active plan_id is invalid: " + planId.str();
                    return false;
                }
                if (seen.insert(planId).second) {
                    unique.push_back(planId);
                }
            }
            selection.explicitPlanIds = std::move(unique);
            error.clear();
            return true;
        }

        bool contains(
            const std::vector<std::string>& values,
            const std::string& value) {
            return std::find(values.begin(), values.end(), value) != values.end();
        }
    }

    CatalogStore::CatalogStore(FrequencyCatalog& catalog)
        : catalog(catalog) {
        loadedUserDocument.schemaVersion = CATALOG_SCHEMA_VERSION;
    }

    bool CatalogStore::initialize(
        const std::filesystem::path& resourcesDirectory,
        const std::filesystem::path& applicationDataDirectory,
        std::string& error) {
        std::lock_guard<std::mutex> lock(mutex);
        if (isInitialized) {
            error.clear();
            return true;
        }

        const std::filesystem::path resourceDirectory =
            resourcesDirectory / "frequency_catalog";
        std::vector<uint8_t> manifestBytes;
        if (!readBoundedFile(
                resourceDirectory / "manifest-v1.json",
                MAX_MANIFEST_SIZE,
                manifestBytes,
                error)) {
            error = "could not load frequency-catalog manifest: " + error;
            return false;
        }

        RuntimeManifest manifest;
        try {
            json manifestJson = json::parse(
                manifestBytes.begin(),
                manifestBytes.end());
            manifest = parseRuntimeManifest(manifestJson);
        }
        catch (const std::exception& exception) {
            error = std::string("could not parse frequency-catalog manifest: ")
                + exception.what();
            return false;
        }

        std::vector<uint8_t> catalogBytes;
        const std::filesystem::path systemPath =
            resourceDirectory / manifest.catalog.path;
        if (!readBoundedFile(
                systemPath,
                MAX_SYSTEM_CATALOG_SIZE,
                catalogBytes,
                error)) {
            error = "could not load packaged system catalog: " + error;
            return false;
        }
        if (catalogBytes.size() != manifest.catalog.size) {
            error = "packaged system catalog size does not match its manifest";
            return false;
        }
        if (sha256(catalogBytes) != manifest.catalog.sha256) {
            error = "packaged system catalog checksum does not match its manifest";
            return false;
        }

        CatalogDocument systemDocument;
        try {
#ifdef __ANDROID__
            systemDocument = catalogDocumentFromCbor(catalogBytes);
#else
            json systemJson = json::parse(
                catalogBytes.begin(),
                catalogBytes.end());
            systemDocument = catalogDocumentFromJson(std::move(systemJson));
#endif
        }
        catch (const std::exception& exception) {
            error = std::string("could not parse packaged system catalog: ")
                + exception.what();
            return false;
        }
        if (systemDocument.plans.size() != manifest.plans
            || systemDocument.bands.size() != manifest.bands
            || systemDocument.segments.size() != manifest.segments
            || systemDocument.bookmarks.size() != manifest.bookmarks) {
            error = "packaged system catalog counts do not match its manifest";
            return false;
        }
        if (!catalog.canReplaceLayer(
                CatalogLayer::System,
                systemDocument,
                error)) {
            error = "packaged system catalog is not publishable: " + error;
            return false;
        }
        if (!catalog.replaceLayer(
                CatalogLayer::System,
                std::move(systemDocument),
                error)) {
            error = "could not publish packaged system catalog: " + error;
            return false;
        }

        dataDirectory = applicationDataDirectory;
        loadedSourceInfo = std::move(manifest.source);
        warnings.clear();

        CatalogDocument userDocument;
        userDocument.schemaVersion = CATALOG_SCHEMA_VERSION;
        const std::filesystem::path userPath = dataDirectory / "user-v1.json";
        std::error_code inspectError;
        bool userExists = std::filesystem::exists(userPath, inspectError);
        if (inspectError) {
            warnings.push_back(
                "Could not inspect the user catalog; using an empty user layer: "
                + inspectError.message());
        }
        else if (userExists) {
            std::vector<uint8_t> userBytes;
            std::string userError;
            if (!readBoundedFile(
                    userPath,
                    MAX_USER_CATALOG_SIZE,
                    userBytes,
                    userError)) {
                warnings.push_back(
                    "Could not load the user catalog; using an empty user layer: "
                    + userError);
            }
            else {
                try {
                    json userJson = json::parse(
                        userBytes.begin(),
                        userBytes.end());
                    CatalogDocument candidate =
                        catalogDocumentFromJson(std::move(userJson));
                    if (candidate.bookmarks.size() > MAX_USER_BOOKMARKS) {
                        throw std::invalid_argument(
                            "user catalog exceeds the 50000-bookmark limit");
                    }
                    if (!catalog.canReplaceLayer(
                            CatalogLayer::User,
                            candidate,
                            userError)) {
                        throw std::invalid_argument(userError);
                    }
                    userDocument = std::move(candidate);
                }
                catch (const std::exception& exception) {
                    warnings.push_back(
                        std::string(
                            "Could not parse the user catalog; using an empty "
                            "user layer and preserving the invalid file: ")
                        + exception.what());
                }
            }
        }
        if (!catalog.replaceLayer(
                CatalogLayer::User,
                userDocument,
                error)) {
            error = "could not publish user catalog: " + error;
            return false;
        }
        loadedUserDocument = std::move(userDocument);

        CatalogContextSelection selection;
        const std::filesystem::path contextPath =
            dataDirectory / "context-v1.json";
        inspectError.clear();
        bool contextExists = std::filesystem::exists(contextPath, inspectError);
        if (inspectError) {
            warnings.push_back(
                "Could not inspect catalog context; using General: "
                + inspectError.message());
        }
        else if (contextExists) {
            std::vector<uint8_t> contextBytes;
            std::string contextError;
            if (!readBoundedFile(
                    contextPath,
                    MAX_CONTEXT_SIZE,
                    contextBytes,
                    contextError)) {
                warnings.push_back(
                    "Could not load catalog context; using General: "
                    + contextError);
            }
            else {
                try {
                    json contextJson = json::parse(
                        contextBytes.begin(),
                        contextBytes.end());
                    selection = contextJson.get<CatalogContextSelection>();
                    if (!normalizeSelection(selection, contextError)) {
                        throw std::invalid_argument(contextError);
                    }
                }
                catch (const std::exception& exception) {
                    selection = {};
                    warnings.push_back(
                        std::string(
                            "Could not parse catalog context; using General "
                            "and preserving the invalid file: ")
                        + exception.what());
                }
            }
        }
        loadedSelection = std::move(selection);
        resolvedContext = resolveContextLocked(loadedSelection);
        isInitialized = true;
        error.clear();
        return true;
    }

    bool CatalogStore::initialized() const {
        std::lock_guard<std::mutex> lock(mutex);
        return isInitialized;
    }

    CatalogSourceInfo CatalogStore::sourceInfo() const {
        std::lock_guard<std::mutex> lock(mutex);
        return loadedSourceInfo;
    }

    CatalogDocument CatalogStore::userDocument() const {
        std::lock_guard<std::mutex> lock(mutex);
        return loadedUserDocument;
    }

    CatalogContextSelection CatalogStore::contextSelection() const {
        std::lock_guard<std::mutex> lock(mutex);
        return loadedSelection;
    }

    CatalogContext CatalogStore::activeContext() const {
        std::lock_guard<std::mutex> lock(mutex);
        return resolvedContext;
    }

    std::vector<std::string> CatalogStore::recoveryWarnings() const {
        std::lock_guard<std::mutex> lock(mutex);
        return warnings;
    }

    bool CatalogStore::commitUserDocument(
        CatalogDocument document,
        std::string& error) {
        if (!isInitialized) {
            error = "frequency catalog store is not initialized";
            return false;
        }
        if (document.bookmarks.size() > MAX_USER_BOOKMARKS) {
            error = "user catalog exceeds the 50000-bookmark limit";
            return false;
        }
        if (!catalog.canReplaceLayer(
                CatalogLayer::User,
                document,
                error)) {
            return false;
        }
        std::string serialized;
        try {
            serialized = catalogDocumentToJson(document).dump(2);
            serialized.push_back('\n');
        }
        catch (const std::exception& exception) {
            error = std::string("could not serialize user catalog: ")
                + exception.what();
            return false;
        }
        if (!writeAtomic(
                dataDirectory / "user-v1.json",
                serialized,
                MAX_USER_CATALOG_SIZE,
                error)) {
            return false;
        }
        // canReplaceLayer validated this exact document while the store mutex
        // excluded another static-layer write.
        if (!catalog.replaceLayer(
                CatalogLayer::User,
                document,
                error)) {
            error = "user catalog was persisted but could not be published: "
                + error;
            return false;
        }
        loadedUserDocument = std::move(document);
        resolvedContext = resolveContextLocked(loadedSelection);
        error.clear();
        return true;
    }

    bool CatalogStore::upsertUserBookmark(
        Bookmark bookmark,
        std::string& error) {
        std::lock_guard<std::mutex> lock(mutex);
        if (!isInitialized) {
            error = "frequency catalog store is not initialized";
            return false;
        }
        bookmark.layer = CatalogLayer::User;
        CatalogDocument document = loadedUserDocument;
        auto existing = std::find_if(
            document.bookmarks.begin(),
            document.bookmarks.end(),
            [&](const Bookmark& candidate) {
                return candidate.bookmarkId == bookmark.bookmarkId;
            });
        if (existing == document.bookmarks.end()) {
            document.bookmarks.push_back(std::move(bookmark));
        }
        else {
            *existing = std::move(bookmark);
        }
        return commitUserDocument(std::move(document), error);
    }

    bool CatalogStore::removeUserBookmark(
        const BookmarkId& bookmarkId,
        std::string& error) {
        std::lock_guard<std::mutex> lock(mutex);
        if (!isInitialized) {
            error = "frequency catalog store is not initialized";
            return false;
        }
        if (!isValidStableId(bookmarkId.str())) {
            error = "bookmark_id is invalid";
            return false;
        }
        CatalogDocument document = loadedUserDocument;
        auto newEnd = std::remove_if(
            document.bookmarks.begin(),
            document.bookmarks.end(),
            [&](const Bookmark& bookmark) {
                return bookmark.bookmarkId == bookmarkId;
            });
        if (newEnd == document.bookmarks.end()) {
            error.clear();
            return true;
        }
        document.bookmarks.erase(newEnd, document.bookmarks.end());
        return commitUserDocument(std::move(document), error);
    }

    bool CatalogStore::replaceUserBookmarks(
        std::vector<Bookmark> bookmarks,
        std::string& error) {
        std::lock_guard<std::mutex> lock(mutex);
        if (!isInitialized) {
            error = "frequency catalog store is not initialized";
            return false;
        }
        for (Bookmark& bookmark : bookmarks) {
            bookmark.layer = CatalogLayer::User;
        }
        CatalogDocument document = loadedUserDocument;
        document.bookmarks = std::move(bookmarks);
        return commitUserDocument(std::move(document), error);
    }

    bool CatalogStore::setContextSelection(
        CatalogContextSelection selection,
        std::string& error) {
        std::lock_guard<std::mutex> lock(mutex);
        if (!isInitialized) {
            error = "frequency catalog store is not initialized";
            return false;
        }
        if (!normalizeSelection(selection, error)) {
            return false;
        }
        std::string serialized = json(selection).dump(2);
        serialized.push_back('\n');
        if (!writeAtomic(
                dataDirectory / "context-v1.json",
                serialized,
                MAX_CONTEXT_SIZE,
                error)) {
            return false;
        }
        loadedSelection = std::move(selection);
        resolvedContext = resolveContextLocked(loadedSelection);
        error.clear();
        return true;
    }

    CatalogContext CatalogStore::resolveContextLocked(
        const CatalogContextSelection& selection) const {
        CatalogContext result;
        result.countryCode = selection.countryCode;
        result.subdivision = selection.subdivision;
        if (selection.ituRegion >= 1 && selection.ituRegion <= 3) {
            result.ituRegionMask =
                static_cast<uint8_t>(1U << (selection.ituRegion - 1));
        }
        const bool regionWasSelected = result.ituRegionMask != 0;

        std::shared_ptr<const CatalogSnapshot> snapshot = catalog.snapshot();
        if (!snapshot) {
            return result;
        }
        auto appendPlan = [&](const BandPlan& plan) {
            if (std::find(
                    result.activePlans.begin(),
                    result.activePlans.end(),
                    plan.planId) == result.activePlans.end()) {
                result.activePlans.push_back(plan.planId);
                if (!regionWasSelected) {
                    result.ituRegionMask =
                        static_cast<uint8_t>(
                            result.ituRegionMask | plan.scope.ituRegionMask);
                }
            }
        };

        for (const PlanId& planId : selection.explicitPlanIds) {
            const BandPlan* plan = snapshot->findPlan(planId);
            if (plan) {
                appendPlan(*plan);
            }
        }
        if (!result.activePlans.empty()) {
            return result;
        }

        if (!selection.subdivision.empty()) {
            for (const BandPlan& plan : snapshot->plans) {
                if (contains(plan.scope.subdivisions, selection.subdivision)
                    && (plan.scope.countryCodes.empty()
                        || contains(
                            plan.scope.countryCodes,
                            selection.countryCode))) {
                    appendPlan(plan);
                }
            }
        }
        if (result.activePlans.empty() && !selection.countryCode.empty()) {
            for (const BandPlan& plan : snapshot->plans) {
                if (plan.scope.subdivisions.empty()
                    && contains(
                        plan.scope.countryCodes,
                        selection.countryCode)) {
                    appendPlan(plan);
                }
            }
        }
        if (!result.activePlans.empty()) {
            return result;
        }

        if (result.ituRegionMask != 0) {
            for (const BandPlan& plan : snapshot->plans) {
                if (plan.scope.countryCodes.empty()
                    && plan.scope.subdivisions.empty()
                    && (plan.scope.ituRegionMask & result.ituRegionMask) != 0) {
                    appendPlan(plan);
                }
            }
        }
        if (!result.activePlans.empty()) {
            return result;
        }

        // General is a fallback, never an inferred owner of a frequency. Prefer
        // the generated stable ID, then any globally scoped plan.
        const BandPlan* general =
            snapshot->findPlan(PlanId("plan:openwebrx:default"));
        if (general) {
            appendPlan(*general);
            return result;
        }
        for (const BandPlan& plan : snapshot->plans) {
            if (isEmptyScope(plan.scope)) {
                appendPlan(plan);
            }
        }
        return result;
    }

    void to_json(json& j, const CatalogContextSelection& value) {
        j = {
            { "schema_version", CATALOG_CONTEXT_SCHEMA_VERSION },
            { "explicit_plan_ids", value.explicitPlanIds },
            { "country_code", value.countryCode },
            { "subdivision", value.subdivision },
            { "itu_region", value.ituRegion }
        };
    }

    void from_json(const json& j, CatalogContextSelection& value) {
        if (j.at("schema_version").get<int>()
            != CATALOG_CONTEXT_SCHEMA_VERSION) {
            throw std::invalid_argument(
                "catalog context schema is unsupported");
        }
        value.explicitPlanIds =
            j.value("explicit_plan_ids", std::vector<PlanId>{});
        value.countryCode = j.value("country_code", "");
        value.subdivision = j.value("subdivision", "");
        value.ituRegion = j.value("itu_region", 0);
    }

}

namespace core {
    frequency_catalog::CatalogStore& getFrequencyCatalogStore() {
        static frequency_catalog::CatalogStore store(getFrequencyCatalog());
        return store;
    }
}
