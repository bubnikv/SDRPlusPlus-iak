#include <config.h>
#include <utils/flog.h>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <mutex>
#include <system_error>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#if OPT_CONFIG_MULTIPLE_INSTANCES
#include <fcntl.h>
#include <sys/file.h>
#endif
#endif

namespace {
    std::atomic<uint64_t> temporaryFileSequence{ 0 };

#if OPT_CONFIG_MULTIPLE_INSTANCES
    std::mutex configCommitMutex;

    std::filesystem::path normalizeRoot(const std::filesystem::path& root) {
        std::error_code ec;
        std::filesystem::path normalized = std::filesystem::weakly_canonical(root, ec);
        if (ec) { normalized = std::filesystem::absolute(root).lexically_normal(); }
        return normalized;
    }

    // This is the one config commit lock. The in-process mutex serializes the
    // saver with explicit save()/load() calls; desktop builds additionally lock
    // one sidecar for the complete config root so other SDR++ processes join the
    // same serialization domain. The OS releases its part after a crash.
    class ConfigCommitLock {
    public:
        explicit ConfigCommitLock(const std::filesystem::path& root)
            : processLock(configCommitMutex) {
            const std::filesystem::path lockPath = normalizeRoot(root) / ".sdrpp-config.lock";
#ifdef _WIN32
            handle = CreateFileW(
                lockPath.c_str(),
                GENERIC_READ | GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                NULL,
                OPEN_ALWAYS,
                FILE_ATTRIBUTE_NORMAL,
                NULL);
            if (handle == INVALID_HANDLE_VALUE) {
                flog::error("Could not open config commit lock '{}': Windows error {}",
                            lockPath.string(), GetLastError());
                return;
            }
            if (!LockFileEx(handle, LOCKFILE_EXCLUSIVE_LOCK, 0, 1, 0, &overlap)) {
                flog::error("Could not acquire config commit lock '{}': Windows error {}",
                            lockPath.string(), GetLastError());
                CloseHandle(handle);
                handle = INVALID_HANDLE_VALUE;
                return;
            }
#else
            fd = open(lockPath.c_str(), O_RDWR | O_CREAT, 0666);
            if (fd < 0) {
                flog::error("Could not open config commit lock '{}': error {}",
                            lockPath.string(), errno);
                return;
            }
            if (flock(fd, LOCK_EX) != 0) {
                flog::error("Could not acquire config commit lock '{}': error {}",
                            lockPath.string(), errno);
                close(fd);
                fd = -1;
                return;
            }
#endif
            acquired = true;
        }

        ConfigCommitLock(const ConfigCommitLock&) = delete;
        ConfigCommitLock& operator=(const ConfigCommitLock&) = delete;

        ~ConfigCommitLock() {
#ifdef _WIN32
            if (handle != INVALID_HANDLE_VALUE) {
                UnlockFileEx(handle, 0, 1, 0, &overlap);
                CloseHandle(handle);
            }
#else
            if (fd >= 0) {
                flock(fd, LOCK_UN);
                close(fd);
            }
#endif
        }

        explicit operator bool() const { return acquired; }

    private:
        std::unique_lock<std::mutex> processLock;
        bool acquired = false;
#ifdef _WIN32
        HANDLE handle = INVALID_HANDLE_VALUE;
        OVERLAPPED overlap{};
#else
        int fd = -1;
#endif
    };
#endif

    enum class ReadResult {
        OK,
        MISSING,
        INVALID
    };

    ReadResult readJsonFile(const std::filesystem::path& path, json& out) {
        std::error_code ec;
        const bool exists = std::filesystem::exists(path, ec);
        if (ec) { return ReadResult::INVALID; }
        if (!exists) { return ReadResult::MISSING; }
        if (ec || !std::filesystem::is_regular_file(path, ec) || ec) {
            return ReadResult::INVALID;
        }

        try {
            std::ifstream file(path, std::ios::binary);
            if (!file) { return ReadResult::INVALID; }
            file >> out;
            return file ? ReadResult::OK : ReadResult::INVALID;
        }
        catch (const std::exception&) {
            return ReadResult::INVALID;
        }
    }

    std::filesystem::path temporaryPathFor(const std::filesystem::path& path) {
#ifdef _WIN32
        const unsigned long processId = GetCurrentProcessId();
#else
        const long processId = static_cast<long>(getpid());
#endif
        const uint64_t sequence = temporaryFileSequence.fetch_add(1, std::memory_order_relaxed);
        std::filesystem::path temporary = path;
        temporary += ".tmp." + std::to_string(processId) + "." + std::to_string(sequence);
        return temporary;
    }

    bool replaceFile(const std::filesystem::path& source,
                     const std::filesystem::path& destination,
                     bool retryTransientLocks) {
#ifdef _WIN32
        constexpr auto retryBudget = std::chrono::seconds(2);
        constexpr auto maximumDelay = std::chrono::milliseconds(500);
        auto delay = std::chrono::milliseconds(20);
        const auto deadline = std::chrono::steady_clock::now() + retryBudget;
        unsigned int attempts = 0;
        DWORD error = ERROR_SUCCESS;

        for (;;) {
            ++attempts;
            if (MoveFileExW(source.c_str(), destination.c_str(),
                            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
                return true;
            }

            error = GetLastError();
            const bool transient =
                error == ERROR_SHARING_VIOLATION ||
                error == ERROR_LOCK_VIOLATION ||
                // Security software sometimes reports a temporary open handle
                // as access denied rather than as a sharing violation.
                error == ERROR_ACCESS_DENIED;
            const auto now = std::chrono::steady_clock::now();
            if (!retryTransientLocks || !transient || now >= deadline) { break; }

            const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                deadline - now);
            std::this_thread::sleep_for((std::min)(delay, remaining));
            delay = (std::min)(delay * 2, maximumDelay);
        }

        flog::error("Could not replace config file '{}' after {} attempt(s): Windows error {}",
                    destination.string(), attempts, error);
        return false;
#else
        (void)retryTransientLocks;
        std::error_code ec;
        std::filesystem::rename(source, destination, ec);
        if (!ec) { return true; }
        flog::error("Could not replace config file '{}': {}", destination.string(), ec.message());
        return false;
#endif
    }

    bool writeJsonAtomically(const std::filesystem::path& path, const json& document,
                             bool retryTransientLocks = false) {
        const std::filesystem::path temporary = temporaryPathFor(path);
        try {
            {
                std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
                if (!file) {
                    flog::error("Could not open temporary config file '{}'", temporary.string());
                    return false;
                }
                file << document.dump(4);
                file.close();
                if (!file) {
                    flog::error("Could not write temporary config file '{}'", temporary.string());
                    std::error_code ignored;
                    std::filesystem::remove(temporary, ignored);
                    return false;
                }
            }

            if (replaceFile(temporary, path, retryTransientLocks)) { return true; }
        }
        catch (const std::exception& e) {
            flog::error("Could not serialize config file '{}': {}", path.string(), e.what());
            std::error_code ignored;
            std::filesystem::remove(temporary, ignored);
            return false;
        }

        {
            std::error_code ignored;
            std::filesystem::remove(temporary, ignored);
        }
        return false;
    }

    struct SaveSnapshot {
        std::string path;
        json local;
        json baseline;
        bool baselineValid = false;
        std::uint64_t revision = 0;
    };

    struct SaveResult {
        bool success = false;
        json document;
    };

    // Apply precisely the local changes made since baseline onto the newest disk
    // document. Objects merge by key (including deletions); arrays and scalar
    // values are atomic. An actual same-key conflict is resolved by the process
    // committing last.
    json mergeLocalChanges(const json& baseline, const json& local, const json& disk) {
        if (baseline == local) { return disk; }
        if ((!baseline.is_object() && !baseline.is_null()) || !local.is_object()) {
            return local;
        }

        const json emptyBaseline = json::object();
        const json& objectBaseline = baseline.is_null() ? emptyBaseline : baseline;
        json merged = disk.is_object() ? disk : objectBaseline;
        for (auto it = objectBaseline.begin(); it != objectBaseline.end(); ++it) {
            if (local.find(it.key()) == local.end()) { merged.erase(it.key()); }
        }

        for (auto it = local.begin(); it != local.end(); ++it) {
            const auto baselineIt = objectBaseline.find(it.key());
            if (baselineIt == objectBaseline.end()) {
                const auto diskIt = merged.find(it.key());
                if (it->is_object() && diskIt != merged.end() && diskIt->is_object()) {
                    merged[it.key()] = mergeLocalChanges(json::object(), *it, *diskIt);
                }
                else {
                    merged[it.key()] = *it;
                }
                continue;
            }
            if (*baselineIt == *it) { continue; }

            const auto diskIt = merged.find(it.key());
            const json& diskValue = diskIt == merged.end() ? *baselineIt : *diskIt;
            merged[it.key()] = mergeLocalChanges(*baselineIt, *it, diskValue);
        }
        return merged;
    }

    SaveResult persistSnapshot(const SaveSnapshot& snapshot, bool retryTransientLocks) {
        SaveResult result;
        if (snapshot.path.empty()) {
            flog::error("Config manager tried to save file with no path specified");
            return result;
        }

        const std::filesystem::path filePath(snapshot.path);
#if OPT_CONFIG_MULTIPLE_INSTANCES
        // A terminal Windows save may wait here for a transient replacement lock
        // to clear. Keep the cross-process lock for that bounded retry: releasing
        // it would let another instance change the disk document after the merge
        // below, requiring a fresh read and merge before every attempt.
        ConfigCommitLock commitLock(filePath.parent_path());
        if (!commitLock) { return result; }

        json disk;
        const ReadResult readResult = readJsonFile(filePath, disk);
        if (readResult != ReadResult::OK) {
            if (readResult == ReadResult::INVALID) {
                flog::error("Config file '{}' became invalid; repairing it from the last valid state",
                            snapshot.path);
            }
            disk = snapshot.baselineValid ? snapshot.baseline : snapshot.local;
        }

        result.document = snapshot.baselineValid
            ? mergeLocalChanges(snapshot.baseline, snapshot.local, disk)
            : snapshot.local;
        if ((readResult != ReadResult::OK || result.document != disk) &&
            !writeJsonAtomically(filePath, result.document, retryTransientLocks)) {
            result.document = json();
            return result;
        }
#else
        result.document = snapshot.local;
        if (!writeJsonAtomically(filePath, result.document, retryTransientLocks)) {
            result.document = json();
            return result;
        }
#endif
        result.success = true;
        return result;
    }
}

class ConfigSaver {
public:
    static ConfigSaver& instance() {
        static ConfigSaver saver;
        return saver;
    }

    void enable(ConfigManager* manager) {
        std::lock_guard<std::mutex> lifecycleGuard(lifecycleMtx);
        {
            std::lock_guard<std::mutex> managerGuard(manager->mtx);
            if (manager->lifecycleState != ConfigManager::LifecycleState::Open) {
                throw std::logic_error("ConfigManager::enableAutoSave called after shutdown began");
            }
            if (manager->autoSaveEnabled.exchange(true, std::memory_order_acq_rel)) { return; }
        }

        {
            std::lock_guard<std::mutex> guard(mtx);
            managers.push_back(std::make_shared<Registration>(manager));
            pending = true;
            if (!worker.joinable()) {
                stopping = false;
                worker = std::thread(&ConfigSaver::run, this);
            }
        }
        cond.notify_one();
    }

    void disable(ConfigManager* manager) {
        std::lock_guard<std::mutex> lifecycleGuard(lifecycleMtx);
        if (!manager->autoSaveEnabled.exchange(false, std::memory_order_acq_rel)) { return; }

        bool join = false;
        std::shared_ptr<Registration> registration;
        {
            std::lock_guard<std::mutex> guard(mtx);
            const auto it = std::find_if(
                managers.begin(), managers.end(),
                [manager](const std::shared_ptr<Registration>& entry) {
                    return entry->manager == manager;
                });
            if (it != managers.end()) {
                registration = *it;
                managers.erase(it);
            }
            if (managers.empty() && worker.joinable()) {
                stopping = true;
                join = true;
            }
        }
        cond.notify_one();

        // A scan keeps this mutex for exactly one save. Waiting here guarantees
        // the worker has stopped using the manager before its destructor returns,
        // without keeping the registry mutex across filesystem I/O.
        if (registration) {
            std::lock_guard<std::mutex> useGuard(registration->useMtx);
            registration->manager = NULL;
        }
        if (join) { worker.join(); }
    }

    void notifyDirty(ConfigManager* manager) {
        if (!manager->autoSaveEnabled.load(std::memory_order_acquire)) { return; }
        {
            std::lock_guard<std::mutex> guard(mtx);
            pending = true;
        }
        cond.notify_one();
    }

    bool flushAll() {
        std::vector<std::shared_ptr<Registration>> scan;
        {
            std::lock_guard<std::mutex> guard(mtx);
            scan = managers;
        }
        return saveManagers(scan);
    }

    ~ConfigSaver() {
        std::lock_guard<std::mutex> lifecycleGuard(lifecycleMtx);
        {
            std::lock_guard<std::mutex> guard(mtx);
            for (const auto& registration : managers) {
                if (registration->manager) {
                    registration->manager->autoSaveEnabled.store(false, std::memory_order_release);
                }
            }
            stopping = true;
        }
        cond.notify_one();
        if (worker.joinable()) { worker.join(); }
        managers.clear();
    }

private:
    struct Registration {
        explicit Registration(ConfigManager* manager) : manager(manager) {}

        std::mutex useMtx;
        ConfigManager* manager;
    };

    static bool saveManagers(
        const std::vector<std::shared_ptr<Registration>>& scan) {
        bool allSaved = true;
        for (const auto& registration : scan) {
            std::lock_guard<std::mutex> useGuard(registration->useMtx);
            if (registration->manager && !registration->manager->saveIfDirty()) {
                allSaved = false;
            }
        }
        return allSaved;
    }

    void run() {
        std::unique_lock<std::mutex> guard(mtx);
        while (!stopping) {
            cond.wait(guard, [this]() { return stopping || pending; });
            if (stopping) { break; }

            // Coalesce a burst of UI changes into one save. Notifications during
            // this interval wake the condition variable but do not extend it.
            if (cond.wait_for(guard, std::chrono::seconds(1),
                              [this]() { return stopping; })) {
                break;
            }

            // Everything notified before this snapshot is covered by this scan.
            // A mutation during the unlocked scan sets pending again.
            pending = false;
            const auto scan = managers;
            guard.unlock();
            const bool allSaved = saveManagers(scan);
            guard.lock();
            pending |= !allSaved;
        }
    }

    std::mutex lifecycleMtx;
    std::mutex mtx;
    std::condition_variable cond;
    std::vector<std::shared_ptr<Registration>> managers;
    std::thread worker;
    bool stopping = false;
    bool pending = false;
};

namespace config_detail {
    void logTypeMismatch(std::string_view key, const char* what) {
        flog::warn("Config entry '{}' could not be read: {}",
                   std::string(key), what);
    }

    void logDefaultRepair(std::string_view key) {
        flog::warn("Config entry '{}' is incompatible; replacing it with its default",
                   std::string(key));
    }
}

namespace {
    // Defined in the core DLL so every plugin on this thread observes the same
    // active access. An inline thread_local in config.h would be duplicated
    // across dynamic-library boundaries on some platforms. Only one manager may
    // be held at a time, which prevents both self-deadlock and cross-manager ABBA.
    thread_local ConfigManager::ReadAccess* activeConfigAccess = NULL;
}

ConfigManager::ConfigManager() {
}

ConfigManager::~ConfigManager() {
    // Destruction cannot report a nesting error, but it must still unregister
    // this manager from the process-wide saver before its storage disappears.
    disableAutoSaveImpl();
    bool unsaved = false;
    std::string unsavedPath;
    {
        std::lock_guard<std::mutex> guard(mtx);
        unsaved = dirty;
        if (unsaved) { unsavedPath = path; }
    }
    if (unsaved) {
        flog::error("Config manager for '{}' was destroyed with unsaved changes", unsavedPath);
    }
}

void ConfigManager::setPath(std::string file) {
    if (anyAccessHeldByCurrentThread()) {
        throw std::logic_error("ConfigManager::setPath called while a config access is active");
    }
    const std::string newPath =
        std::filesystem::absolute(std::move(file)).lexically_normal().string();
    bool shouldSave = false;
    {
        std::lock_guard<std::mutex> persistenceGuard(persistenceMtx);
        std::lock_guard<std::mutex> guard(mtx);
        if (lifecycleState != LifecycleState::Open) {
            throw std::logic_error("ConfigManager::setPath called after shutdown began");
        }
        if (path != newPath) {
            path = newPath;
            baseline = json();
            baselineValid = false;
            // setPath() normally precedes load(). Do not let an autosaver that
            // was enabled unusually early serialize an uninitialized null
            // document over the destination.
            dirty = !conf.is_null();
            ++revision;
            shouldSave = dirty;
        }
    }
    if (shouldSave && autoSaveEnabled.load(std::memory_order_acquire)) {
        ConfigSaver::instance().notifyDirty(this);
    }
}

void ConfigManager::load(json def) {
    if (anyAccessHeldByCurrentThread()) {
        throw std::logic_error("ConfigManager::load called while a config access is active");
    }
    std::lock_guard<std::mutex> persistenceGuard(persistenceMtx);
    std::string loadPath;
    json loadBaseline;
    std::uint64_t loadRevision = 0;
    {
        std::lock_guard<std::mutex> guard(mtx);
        if (lifecycleState != LifecycleState::Open) {
            throw std::logic_error("ConfigManager::load called after shutdown began");
        }
        loadPath = path;
        loadBaseline = conf;
        loadRevision = revision;
    }

    if (loadPath.empty()) {
        flog::error("Config manager tried to load file with no path specified");
        return;
    }

    const std::filesystem::path filePath(loadPath);
    json document;
    bool documentPersisted = false;
#if OPT_CONFIG_MULTIPLE_INSTANCES
    ConfigCommitLock commitLock(filePath.parent_path());
    if (!commitLock) {
        document = std::move(def);
    }
    else
#endif
    {
        json loaded;
        const ReadResult result = readJsonFile(filePath, loaded);
        if (result == ReadResult::OK) {
            document = std::move(loaded);
            documentPersisted = true;
        }
        else {
            if (result == ReadResult::MISSING) {
                flog::warn("Config file '{}' does not exist, creating it", loadPath);
            }
            else {
                flog::error("Config file '{}' is not a valid JSON file, resetting it", loadPath);
            }

            document = std::move(def);
            documentPersisted = writeJsonAtomically(filePath, document);
        }
    }

    {
        std::lock_guard<std::mutex> guard(mtx);
        const bool changedDuringLoad = revision != loadRevision;
        if (changedDuringLoad) {
            conf = mergeLocalChanges(loadBaseline, conf, document);
            baseline = std::move(document);
        }
        else {
            // Keep the committed document as both live state and merge baseline,
            // paying for one full copy rather than two.
            baseline = document;
            conf = std::move(document);
        }
        // Even when the initial write or commit lock failed, the loaded/default
        // document is a valid merge baseline. On retry, unchanged defaults then
        // preserve a newer disk document instead of overwriting it wholesale.
        baselineValid = true;
        // A concurrent edit is conservatively left dirty even if it happened to
        // restore the loaded value. The next autosave will cheaply clear it.
        dirty = !documentPersisted || changedDuringLoad;
        ++revision;
    }
    if (!documentPersisted && autoSaveEnabled.load(std::memory_order_acquire)) {
        ConfigSaver::instance().notifyDirty(this);
    }
}

bool ConfigManager::save() {
    if (anyAccessHeldByCurrentThread()) {
        throw std::logic_error("ConfigManager::save called while a config access is active");
    }
    return saveImpl(false);
}

bool ConfigManager::flushAll() {
    if (activeConfigAccess != NULL) {
        throw std::logic_error("ConfigManager::flushAll called while a config access is active");
    }
    return ConfigSaver::instance().flushAll();
}

bool ConfigManager::saveImpl(bool onlyIfDirty, bool allowClosing,
                             SaveRetryMode retryMode) {
    std::lock_guard<std::mutex> persistenceGuard(persistenceMtx);
    SaveSnapshot snapshot;
    {
        std::lock_guard<std::mutex> guard(mtx);
        if (lifecycleState != LifecycleState::Open) {
            if (!allowClosing) {
                throw std::logic_error("ConfigManager::save called after shutdown began");
            }
            if (lifecycleState == LifecycleState::Closed) { return !dirty; }
        }
        if (onlyIfDirty && !dirty) { return true; }
        snapshot.path = path;
        snapshot.local = conf;
        snapshot.baseline = baseline;
        snapshot.baselineValid = baselineValid;
        snapshot.revision = revision;
    }

    SaveResult result = persistSnapshot(
        snapshot, retryMode == SaveRetryMode::TransientWindowsLocks);
    if (!result.success) { return false; }

    {
        std::lock_guard<std::mutex> guard(mtx);
        const bool changedDuringSave = revision != snapshot.revision;
        if (!changedDuringSave) {
            // result.document may include another process's non-conflicting
            // edits. Keep one copy as the next merge baseline and move the other
            // into the live document.
            baseline = result.document;
            conf = std::move(result.document);
            dirty = false;
        }
        else {
#if OPT_CONFIG_MULTIPLE_INSTANCES
            // Rebase changes made while the file was being written onto the
            // document that was actually committed, including other processes'
            // non-conflicting changes.
            conf = mergeLocalChanges(snapshot.local, conf, result.document);
#endif
            baseline = std::move(result.document);
            // Do not spend another O(document size) pass proving that a
            // concurrent edit was a net no-op. One conservative follow-up save
            // is cheaper and clears this flag exactly.
            dirty = true;
        }
        baselineValid = true;
    }
    return true;
}

bool ConfigManager::saveIfDirty() {
    return saveImpl(true, true);
}

void ConfigManager::enableAutoSave() {
    if (anyAccessHeldByCurrentThread()) {
        throw std::logic_error("ConfigManager::enableAutoSave called while a config access is active");
    }
    ConfigSaver::instance().enable(this);
}

void ConfigManager::disableAutoSave() {
    if (anyAccessHeldByCurrentThread()) {
        throw std::logic_error("ConfigManager::disableAutoSave called while a config access is active");
    }
    {
        std::lock_guard<std::mutex> guard(mtx);
        if (lifecycleState != LifecycleState::Open) {
            throw std::logic_error("ConfigManager::disableAutoSave called after shutdown began");
        }
    }
    disableAutoSaveImpl();
}

void ConfigManager::disableAutoSaveImpl() {
    if (!autoSaveEnabled.load(std::memory_order_acquire)) { return; }
    ConfigSaver::instance().disable(this);
}

bool ConfigManager::shutdown() {
    if (anyAccessHeldByCurrentThread()) {
        throw std::logic_error("ConfigManager::shutdown called while a config access is active");
    }

    {
        std::lock_guard<std::mutex> guard(mtx);
        if (lifecycleState == LifecycleState::Closed) { return true; }
        lifecycleState = LifecycleState::Closing;
    }

    // unregisterAndWait: after this returns the shared saver no longer uses us.
    disableAutoSaveImpl();

    // New accesses are rejected in Closing, so one final dirty-only commit is a
    // stable local snapshot. The cross-process lock still merges other SDR++
    // instances that commit before us.
    if (!saveImpl(true, true, SaveRetryMode::TransientWindowsLocks)) {
        std::string failedPath;
        {
            std::lock_guard<std::mutex> guard(mtx);
            failedPath = path;
        }
        flog::error("Could not save config '{}' during shutdown", failedPath);
        return false;
    }

    bool remainedDirty = false;
    std::string dirtyPath;
    {
        std::lock_guard<std::mutex> guard(mtx);
        if (dirty) {
            remainedDirty = true;
            dirtyPath = path;
        }
        else {
            lifecycleState = LifecycleState::Closed;
        }
    }
    if (remainedDirty) {
        flog::error("Config '{}' remained dirty after its shutdown save", dirtyPath);
        return false;
    }
    return true;
}

void ConfigManager::acquire(ReadAccess* access) {
    if (activeConfigAccess != NULL) {
        throw std::logic_error("nested ConfigManager access");
    }
    mtx.lock();
    if (lifecycleState != LifecycleState::Open) {
        mtx.unlock();
        throw std::logic_error("ConfigManager access attempted after shutdown began");
    }
    activeConfigAccess = access;
}

void ConfigManager::release(ReadAccess* access, bool modified) noexcept {
    if (activeConfigAccess != access) { std::terminate(); }
    activeConfigAccess = NULL;
    if (modified) {
        dirty = true;
        ++revision;
    }
    mtx.unlock();
    if (modified && autoSaveEnabled.load(std::memory_order_acquire)) {
        // Destruction of an EditAccess must never terminate the process because
        // the best-effort wake-up failed. dirty remains set, so a later wake-up
        // or explicit save can still persist the edit.
        try {
            ConfigSaver::instance().notifyDirty(this);
        }
        catch (...) {}
    }
}

bool ConfigManager::anyAccessHeldByCurrentThread() const noexcept {
    return activeConfigAccess != NULL;
}
