#include <config.h>
#include <utils/flog.h>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <system_error>

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

    bool replaceFile(const std::filesystem::path& source, const std::filesystem::path& destination) {
#ifdef _WIN32
        if (MoveFileExW(source.c_str(), destination.c_str(),
                        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            return true;
        }
        flog::error("Could not replace config file '{}': Windows error {}",
                    destination.string(), GetLastError());
        return false;
#else
        std::error_code ec;
        std::filesystem::rename(source, destination, ec);
        if (!ec) { return true; }
        flog::error("Could not replace config file '{}': {}", destination.string(), ec.message());
        return false;
#endif
    }

    bool writeJsonAtomically(const std::filesystem::path& path, const json& document) {
        const std::filesystem::path temporary = temporaryPathFor(path);
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

        if (replaceFile(temporary, path)) { return true; }
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        return false;
    }

#if OPT_CONFIG_MULTIPLE_INSTANCES
    // Apply precisely the local changes made since baseline onto the newest disk
    // document. Objects merge by key (including deletions); arrays and scalar
    // values are atomic. An actual same-key conflict is resolved by the process
    // committing last.
    json mergeLocalChanges(const json& baseline, const json& local, const json& disk) {
        if (baseline == local) { return disk; }
        if (!baseline.is_object() || !local.is_object()) { return local; }

        json merged = disk.is_object() ? disk : baseline;
        for (auto it = baseline.begin(); it != baseline.end(); ++it) {
            if (local.find(it.key()) == local.end()) { merged.erase(it.key()); }
        }

        for (auto it = local.begin(); it != local.end(); ++it) {
            const auto baselineIt = baseline.find(it.key());
            if (baselineIt == baseline.end()) {
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
#endif
}

class ConfigSaver {
public:
    static ConfigSaver& instance() {
        static ConfigSaver saver;
        return saver;
    }

    void enable(ConfigManager* manager) {
        std::lock_guard<std::mutex> lifecycleGuard(lifecycleMtx);
        if (manager->autoSaveEnabled.exchange(true, std::memory_order_acq_rel)) { return; }

        {
            std::lock_guard<std::mutex> guard(mtx);
            managers.push_back(manager);
            if (!worker.joinable()) {
                stopping = false;
                worker = std::thread(&ConfigSaver::run, this);
            }
        }
    }

    void disable(ConfigManager* manager) {
        std::lock_guard<std::mutex> lifecycleGuard(lifecycleMtx);
        if (!manager->autoSaveEnabled.exchange(false, std::memory_order_acq_rel)) { return; }

        bool join = false;
        {
            std::lock_guard<std::mutex> guard(mtx);
            managers.erase(std::remove(managers.begin(), managers.end(), manager), managers.end());
            if (managers.empty() && worker.joinable()) {
                stopping = true;
                join = true;
            }
        }
        cond.notify_one();
        if (join) { worker.join(); }
    }

    ~ConfigSaver() {
        {
            std::lock_guard<std::mutex> lifecycleGuard(lifecycleMtx);
            {
                std::lock_guard<std::mutex> guard(mtx);
                for (ConfigManager* manager : managers) {
                    manager->autoSaveEnabled.store(false, std::memory_order_release);
                }
                managers.clear();
                stopping = true;
            }
            cond.notify_one();
            if (worker.joinable()) { worker.join(); }
        }
    }

private:
    void run() {
        std::unique_lock<std::mutex> guard(mtx);
        while (!stopping) {
            if (cond.wait_for(guard, std::chrono::seconds(1), [this]() { return stopping; })) {
                break;
            }
            // Keep the registry locked while using its raw pointers. Disabling a
            // manager waits for the current scan, so its destructor cannot race us.
            for (ConfigManager* manager : managers) { manager->saveIfDirty(); }
        }
    }

    std::mutex lifecycleMtx;
    std::mutex mtx;
    std::condition_variable cond;
    std::vector<ConfigManager*> managers;
    std::thread worker;
    bool stopping = false;
};

namespace config_detail {
    void logTypeMismatch(std::string_view key, const char* what) {
        flog::warn("Config entry '{}' has an unexpected type, keeping the default: {}",
                   std::string(key), what);
    }
}

ConfigManager::ConfigManager() {
}

ConfigManager::~ConfigManager() {
    disableAutoSave();
}

void ConfigManager::setPath(std::string file) {
    path = std::filesystem::absolute(std::move(file)).lexically_normal().string();
}

void ConfigManager::load(json def, bool lock) {
    std::unique_lock<std::mutex> guard(mtx, std::defer_lock);
    if (lock) { guard.lock(); }

    if (path.empty()) {
        flog::error("Config manager tried to load file with no path specified");
        return;
    }

    const std::filesystem::path filePath(path);
#if OPT_CONFIG_MULTIPLE_INSTANCES
    ConfigCommitLock commitLock(filePath.parent_path());
    if (!commitLock) {
        conf = std::move(def);
        persistedValid = false;
        dirty = true;
        return;
    }
#endif

    json loaded;
    const ReadResult result = readJsonFile(filePath, loaded);
    if (result == ReadResult::OK) {
        conf = std::move(loaded);
        persisted = conf;
        persistedValid = true;
        dirty = false;
        return;
    }

    if (result == ReadResult::MISSING) {
        flog::warn("Config file '{}' does not exist, creating it", path);
    }
    else {
        flog::error("Config file '{}' is not a valid JSON file, resetting it", path);
    }

    conf = std::move(def);
    if (writeJsonAtomically(filePath, conf)) {
        persisted = conf;
        persistedValid = true;
        dirty = false;
    }
    else {
        persistedValid = false;
        dirty = true;
    }
}

void ConfigManager::save(bool lock) {
    std::unique_lock<std::mutex> guard(mtx, std::defer_lock);
    if (lock) { guard.lock(); }
    saveLocked();
}

bool ConfigManager::saveLocked() {
    if (path.empty()) {
        flog::error("Config manager tried to save file with no path specified");
        return false;
    }

    const std::filesystem::path filePath(path);
#if OPT_CONFIG_MULTIPLE_INSTANCES
    ConfigCommitLock commitLock(filePath.parent_path());
    if (!commitLock) { return false; }

    json disk;
    const ReadResult readResult = readJsonFile(filePath, disk);
    if (readResult != ReadResult::OK) {
        if (readResult == ReadResult::INVALID) {
            flog::error("Config file '{}' became invalid; repairing it from the last valid state", path);
        }
        disk = persistedValid ? persisted : conf;
    }

    json merged = persistedValid ? mergeLocalChanges(persisted, conf, disk) : conf;
    if ((readResult != ReadResult::OK || merged != disk) &&
        !writeJsonAtomically(filePath, merged)) {
        return false;
    }

    conf = std::move(merged);
    persisted = conf;
    persistedValid = true;
#else
    if (!writeJsonAtomically(filePath, conf)) { return false; }
#endif
    dirty = false;
    return true;
}

bool ConfigManager::saveIfDirty() {
    std::lock_guard<std::mutex> guard(mtx);
    return !dirty || saveLocked();
}

void ConfigManager::enableAutoSave() {
    ConfigSaver::instance().enable(this);
}

void ConfigManager::disableAutoSave() {
    if (!autoSaveEnabled.load(std::memory_order_acquire)) { return; }
    ConfigSaver::instance().disable(this);
}

void ConfigManager::acquire() {
    mtx.lock();
#ifndef NDEBUG
    owner.store(std::this_thread::get_id(), std::memory_order_relaxed);
#endif
}

void ConfigManager::release(bool modified) {
    if (modified) { dirty = true; }
#ifndef NDEBUG
    owner.store(std::thread::id(), std::memory_order_relaxed);
#endif
    mtx.unlock();
}

#ifndef NDEBUG
bool ConfigManager::heldByCurrentThread() const {
    return owner.load(std::memory_order_relaxed) == std::this_thread::get_id();
}
#endif
