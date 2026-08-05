#include <config.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

static_assert(!std::is_move_constructible<ConfigManager::EditAccess>::value,
              "edit accesses must remain immovable");
static_assert(!std::is_move_assignable<ConfigManager::EditAccess>::value,
              "edit accesses must remain immovable");
static_assert(!std::is_move_constructible<ConfigManager::ReadAccess>::value,
              "read accesses must remain immovable");
static_assert(!std::is_move_assignable<ConfigManager::ReadAccess>::value,
              "read accesses must remain immovable");
static_assert(!std::is_constructible<ConfigManager::ReadAccess, ConfigManager*>::value,
              "read access construction must stay sealed behind ConfigManager::read");

template <class T, class = void>
struct HasSet : std::false_type {};

template <class T>
struct HasSet<T, std::void_t<decltype(
    std::declval<T&>().set(std::string_view(), 0))>> : std::true_type {};

static_assert(!HasSet<ConfigManager::ReadAccess>::value,
              "read access must not expose writers");
static_assert(HasSet<ConfigManager::EditAccess>::value,
              "edit access must expose writers");
static_assert(!HasSet<ConfigManager::ReadSection>::value,
              "read sections must not expose writers");
static_assert(HasSet<ConfigManager::EditSection>::value,
              "edit sections must expose writers");

namespace config_detail {
    struct ConfigManagerTestAccess {
        static bool dirty(ConfigManager& config) {
            std::lock_guard<std::mutex> guard(config.mtx);
            return config.dirty;
        }
    };
}

namespace {
    void require(bool condition, const char* message) {
        if (!condition) { throw std::runtime_error(message); }
    }

    template <class Function>
    void requireLogicError(Function&& function, const char* message) {
        bool rejected = false;
        try {
            function();
        }
        catch (const std::logic_error&) {
            rejected = true;
        }
        require(rejected, message);
    }

    json readFile(const std::filesystem::path& path) {
        std::ifstream file(path, std::ios::binary);
        require(bool(file), "could not open saved config");
        json document;
        file >> document;
        require(bool(file), "could not parse saved config");
        return document;
    }

    void testSetPathBeforeLoad(const std::filesystem::path& path) {
        ConfigManager config;
        config.setPath(path.string());
        require(!config_detail::ConfigManagerTestAccess::dirty(config),
                "setPath marked an uninitialized document dirty");
        require(!std::filesystem::exists(path),
                "setPath wrote an uninitialized null document");
    }

    void testAccessAndLifetime(const std::filesystem::path& path) {
        ConfigManager config;
        config.setPath(path.string());
        config.load(json::object({ { "name", "default" } }));

        require(config.read().value("name", "fallback") == "default",
                "string-literal default returned the wrong value");

        {
            auto configAccess = config.edit();
            require(configAccess.section("missing").peek() == NULL,
                    "a missing read materialized its section");

            requireLogicError([&]() { config.edit().set("nested", true); },
                              "a nested edit access was not rejected");
            requireLogicError([&]() { config.read().contains("nested"); },
                              "a read nested in an edit was not rejected");
            requireLogicError([&]() { config.save(); },
                              "saving from an access was not rejected");
            requireLogicError([&]() { config.load(json::object()); },
                              "loading from an access was not rejected");
            requireLogicError([&]() { config.setPath(path.string()); },
                              "changing path from an access was not rejected");
            requireLogicError([&]() { config.enableAutoSave(); },
                              "enabling autosave from an access was not rejected");
            requireLogicError([&]() { config.disableAutoSave(); },
                              "disabling autosave from an access was not rejected");
            requireLogicError([&]() { ConfigManager::flushAll(); },
                              "flushing all configs from an access was not rejected");
            requireLogicError([&]() { config.shutdown(); },
                              "shutdown from an access was not rejected");
        }

        {
            auto access = config.read();
            requireLogicError([&]() { config.read().contains("name"); },
                              "a nested read access was not rejected");
            requireLogicError([&]() { config.edit().set("nested", true); },
                              "an edit nested in a read was not rejected");
        }

        std::unique_ptr<ConfigManager::EditSection> escaped;
        {
            auto configAccess = config.edit();
            escaped = std::make_unique<ConfigManager::EditSection>(configAccess.section("expired"));
        }
        bool expiredRejected = false;
        try {
            escaped->exists();
        }
        catch (const std::logic_error&) {
            expiredRejected = true;
        }
        require(expiredRejected, "an expired section remained usable");

        config.edit().set("fraction", 3.5);
        int integer = 17;
        require(!config.read().tryGet("fraction", integer) && integer == 17,
                "a floating-point value was truncated into an integer");
        require(config.edit().ensure("fraction", 7),
                "an incompatible numeric default did not report a repair");
        require(config.read().value("fraction", 0) == 7,
                "an incompatible numeric default did not repair the value");

        config.edit().set("oversized", 300);
        std::uint8_t smallInteger = 19;
        require(!config.read().tryGet("oversized", smallInteger) && smallInteger == 19,
                "an out-of-range integer was narrowed");
        require(config.edit().ensure("oversized", std::uint8_t{ 7 }),
                "an out-of-range integer default did not report a repair");
        require(config.read().value("oversized", std::uint8_t{ 0 }) == 7,
                "an out-of-range integer default was not repaired");

        require(config.save(), "could not save access test config");
        require(!readFile(path).contains("missing"),
                "a missing read was persisted as an empty section");

        {
            auto configAccess = config.edit();
            configAccess.set("erased", true);
            require(configAccess.erase("erased"), "erase did not report a removed key");
            require(!configAccess.erase("erased"), "erase reported a missing key as removed");
            require(configAccess.reset(json::object({ { "reset", true } })),
                    "reset did not report a changed document");
            require(!configAccess.reset(json::object({ { "reset", true } })),
                    "reset reported an identical document as changed");
        }
        require(config.read().value("reset", false), "reset did not replace the document");
        require(config.shutdown(), "access test config shutdown failed");
    }

    void testCrossManagerNestingRejected() {
        ConfigManager first;
        ConfigManager second;
        {
            auto firstAccess = first.read();
            requireLogicError([&]() { second.read().contains("value"); },
                              "cross-manager read nesting was not rejected");
            requireLogicError([&]() { second.edit().set("value", 1); },
                              "cross-manager edit nesting was not rejected");
            requireLogicError([&]() { second.save(); },
                              "cross-manager save nesting was not rejected");
            requireLogicError([&]() { second.load(json::object()); },
                              "cross-manager load nesting was not rejected");
            requireLogicError([&]() { second.setPath("cross-manager.json"); },
                              "cross-manager path change was not rejected");
            requireLogicError([&]() { second.enableAutoSave(); },
                              "cross-manager autosave enable was not rejected");
            requireLogicError([&]() { second.disableAutoSave(); },
                              "cross-manager autosave disable was not rejected");
            requireLogicError([&]() { second.shutdown(); },
                              "cross-manager shutdown was not rejected");
        }
        require(!second.read().contains("value"),
                "manager remained inaccessible after rejected nesting");
    }

    void testShutdownLifecycle(const std::filesystem::path& path) {
        ConfigManager config;
        config.setPath(path.string());
        config.load(json::object({ { "value", 0 } }));
        config.enableAutoSave();
        config.edit().set("value", 42);

        // Shutdown must not wait for the autosaver's one-second coalescing pass.
        require(config.shutdown(), "config shutdown failed");
        require(readFile(path).value("value", 0) == 42,
                "shutdown did not flush a pending autosave");
        require(config.shutdown(), "closed config shutdown was not idempotent");

        requireLogicError([&]() { config.read().contains("value"); },
                          "read access after shutdown was not rejected");
        requireLogicError([&]() { config.edit().set("value", 43); },
                          "edit access after shutdown was not rejected");
        requireLogicError([&]() { config.save(); },
                          "save after shutdown was not rejected");
        requireLogicError([&]() { config.load(json::object()); },
                          "load after shutdown was not rejected");
        requireLogicError([&]() { config.setPath(path.string()); },
                          "setPath after shutdown was not rejected");
        requireLogicError([&]() { config.enableAutoSave(); },
                          "autosave enable after shutdown was not rejected");
        requireLogicError([&]() { config.disableAutoSave(); },
                          "autosave disable after shutdown was not rejected");
    }

    void testShutdownRetry(const std::filesystem::path& missingDirectory) {
        const std::filesystem::path path = missingDirectory / "retry.json";
        ConfigManager config;
        config.setPath(path.string());
        config.load(json::object({ { "value", 7 } }));

        require(!config.shutdown(), "shutdown unexpectedly saved into a missing directory");
        requireLogicError([&]() { config.edit().set("value", 8); },
                          "a failed shutdown did not keep the config closed to edits");

        std::filesystem::create_directories(missingDirectory);
        require(config.shutdown(), "config shutdown retry failed");
        require(readFile(path).value("value", 0) == 7,
                "shutdown retry did not persist the dirty document");
    }

#ifdef _WIN32
    void testShutdownRetriesTransientWindowsLock(const std::filesystem::path& path) {
        ConfigManager config;
        config.setPath(path.string());
        config.load(json::object({ { "value", 0 } }));
        config.edit().set("value", 17);

        // Omitting FILE_SHARE_DELETE prevents MoveFileExW from replacing this
        // existing file until the simulated scanner releases its handle.
        HANDLE lock = CreateFileW(
            path.c_str(),
            GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL);
        require(lock != INVALID_HANDLE_VALUE, "could not lock config for shutdown retry test");

        std::thread unlocker([lock]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(150));
            CloseHandle(lock);
        });
        bool shutdownSucceeded = false;
        try {
            shutdownSucceeded = config.shutdown();
        }
        catch (...) {
            unlocker.join();
            throw;
        }
        unlocker.join();

        require(shutdownSucceeded, "shutdown did not outlast a transient Windows file lock");
        require(readFile(path).value("value", 0) == 17,
                "shutdown retry did not commit the dirty config");
    }

    void testShutdownDoesNotRetryReadOnlyDestination(
        const std::filesystem::path& path) {
        ConfigManager config;
        config.setPath(path.string());
        config.load(json::object({ { "value", 0 } }));
        config.edit().set("value", 23);

        const DWORD attributes = GetFileAttributesW(path.c_str());
        require(attributes != INVALID_FILE_ATTRIBUTES,
                "could not read attributes for shutdown retry test");
        require(SetFileAttributesW(path.c_str(), attributes | FILE_ATTRIBUTE_READONLY),
                "could not make config read-only for shutdown retry test");

        std::thread makeWritable([path, attributes]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(150));
            SetFileAttributesW(path.c_str(), attributes);
        });
        bool firstShutdownSucceeded = false;
        try {
            firstShutdownSucceeded = config.shutdown();
        }
        catch (...) {
            makeWritable.join();
            throw;
        }
        makeWritable.join();

        require(!firstShutdownSucceeded,
                "shutdown retried a structurally read-only destination");
        require(config.shutdown(), "shutdown retry failed after clearing read-only state");
        require(readFile(path).value("value", 0) == 23,
                "shutdown retry lost the config after clearing read-only state");
    }
#endif

    void testConcurrentSave(const std::filesystem::path& path) {
        ConfigManager config;
        config.setPath(path.string());
        config.load(json::object({ { "counter", 0 } }));

        std::atomic<bool> start{ false };
        std::atomic<bool> savesSucceeded{ true };
        std::thread saver([&]() {
            while (!start.load(std::memory_order_acquire)) { std::this_thread::yield(); }
            for (int i = 0; i < 20; ++i) {
                if (!config.save()) { savesSucceeded.store(false, std::memory_order_release); }
            }
        });
        std::thread writer([&]() {
            while (!start.load(std::memory_order_acquire)) { std::this_thread::yield(); }
            for (int i = 1; i <= 200; ++i) { config.edit().set("counter", i); }
        });

        start.store(true, std::memory_order_release);
        saver.join();
        writer.join();
        require(savesSucceeded.load(std::memory_order_acquire), "concurrent save failed");
        require(config.save(), "final concurrent save failed");
        require(readFile(path).value("counter", 0) == 200,
                "an edit made during persistence was lost");
    }

#if OPT_CONFIG_MULTIPLE_INSTANCES
    void testIndependentManagerMerge(const std::filesystem::path& path) {
        ConfigManager first;
        ConfigManager second;
        first.setPath(path.string());
        second.setPath(path.string());
        const json defaults = json::object({
            { "first", 0 }, { "second", 0 }, { "deleted", true }
        });
        first.load(defaults);
        second.load(defaults);

        first.edit().set("first", 1);
        {
            auto configAccess = first.edit();
            configAccess.erase("deleted");
        }
        require(first.save(), "first manager save failed");
        second.edit().set("second", 2);
        require(second.save(), "second manager save failed");

        const json merged = readFile(path);
        require(merged.value("first", 0) == 1 && merged.value("second", 0) == 2 &&
                    !merged.contains("deleted"),
                "independent manager changes were not merged");
    }

    void testConcurrentShutdownMerge(const std::filesystem::path& path) {
        ConfigManager first;
        ConfigManager second;
        first.setPath(path.string());
        second.setPath(path.string());
        const json defaults = json::object({ { "first", 0 }, { "second", 0 } });
        first.load(defaults);
        second.load(defaults);
        first.edit().set("first", 11);
        second.edit().set("second", 22);

        std::atomic<bool> start{ false };
        bool firstClosed = false;
        bool secondClosed = false;
        std::thread firstShutdown([&]() {
            while (!start.load(std::memory_order_acquire)) { std::this_thread::yield(); }
            firstClosed = first.shutdown();
        });
        std::thread secondShutdown([&]() {
            while (!start.load(std::memory_order_acquire)) { std::this_thread::yield(); }
            secondClosed = second.shutdown();
        });
        start.store(true, std::memory_order_release);
        firstShutdown.join();
        secondShutdown.join();

        const json merged = readFile(path);
        require(firstClosed && secondClosed, "a concurrent manager shutdown failed");
        require(merged.value("first", 0) == 11 && merged.value("second", 0) == 22,
                "concurrent shutdown lost an independent instance change");
    }

    void testCleanStaleShutdown(const std::filesystem::path& path) {
        ConfigManager writer;
        ConfigManager stale;
        writer.setPath(path.string());
        stale.setPath(path.string());
        const json defaults = json::object({ { "value", 0 } });
        writer.load(defaults);
        stale.load(defaults);

        writer.edit().set("value", 9);
        require(writer.shutdown(), "writer shutdown failed");
        require(stale.shutdown(), "clean stale shutdown failed");
        require(readFile(path).value("value", 0) == 9,
                "a clean stale shutdown reverted a newer instance");
    }
#endif

    bool waitForValue(const std::filesystem::path& path, const char* key, int expected) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(4);
        while (std::chrono::steady_clock::now() < deadline) {
            try {
                if (readFile(path).value(key, 0) == expected) { return true; }
            }
            catch (const std::exception&) {}
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
        }
        return false;
    }

    void testSharedAutoSaver(const std::filesystem::path& firstPath,
                             const std::filesystem::path& secondPath) {
        ConfigManager first;
        ConfigManager second;
        first.setPath(firstPath.string());
        second.setPath(secondPath.string());
        first.load(json::object({ { "value", 0 } }));
        second.load(json::object({ { "value", 0 } }));
        first.enableAutoSave();
        second.enableAutoSave();

        first.edit().set("value", 11);
        second.edit().set("value", 22);
        const bool firstSaved = waitForValue(firstPath, "value", 11);
        const bool secondSaved = waitForValue(secondPath, "value", 22);

        require(first.shutdown(), "first autosaved manager shutdown failed");
        require(second.shutdown(), "second autosaved manager shutdown failed");
        require(firstSaved && secondSaved, "the shared autosaver did not persist both managers");
    }

    void testFlushAll(const std::filesystem::path& firstPath,
                      const std::filesystem::path& secondPath) {
        ConfigManager first;
        ConfigManager second;
        first.setPath(firstPath.string());
        second.setPath(secondPath.string());
        first.load(json::object({ { "value", 0 } }));
        second.load(json::object({ { "value", 0 } }));
        first.enableAutoSave();
        second.enableAutoSave();

        first.edit().set("value", 31);
        second.edit().set("value", 32);

        std::atomic<bool> start{ false };
        bool firstFlush = false;
        bool secondFlush = false;
        std::thread firstFlusher([&]() {
            while (!start.load(std::memory_order_acquire)) { std::this_thread::yield(); }
            firstFlush = ConfigManager::flushAll();
        });
        std::thread secondFlusher([&]() {
            while (!start.load(std::memory_order_acquire)) { std::this_thread::yield(); }
            secondFlush = ConfigManager::flushAll();
        });
        start.store(true, std::memory_order_release);
        firstFlusher.join();
        secondFlusher.join();

        require(firstFlush && secondFlush, "a concurrent all-config flush failed");
        require(readFile(firstPath).value("value", 0) == 31,
                "all-config flush missed the first manager");
        require(readFile(secondPath).value("value", 0) == 32,
                "all-config flush missed the second manager");

        first.edit().set("afterFlush", true);
        require(ConfigManager::flushAll(), "a manager could not be flushed again");
        require(readFile(firstPath).value("afterFlush", false),
                "a manager was not editable after an all-config flush");

        require(first.shutdown(), "first explicitly flushed manager shutdown failed");
        require(second.shutdown(), "second explicitly flushed manager shutdown failed");
    }

    void testFlushAllContinuesAfterFailure(
        const std::filesystem::path& missingDirectory,
        const std::filesystem::path& validPath) {
        ConfigManager failing;
        ConfigManager valid;
        const std::filesystem::path failingPath = missingDirectory / "failing.json";
        failing.setPath(failingPath.string());
        valid.setPath(validPath.string());
        failing.load(json::object({ { "value", 0 } }));
        valid.load(json::object({ { "value", 0 } }));
        failing.enableAutoSave();
        valid.enableAutoSave();

        failing.edit().set("value", 41);
        valid.edit().set("value", 42);
        require(!ConfigManager::flushAll(),
                "all-config flush ignored an individual save failure");
        require(readFile(validPath).value("value", 0) == 42,
                "all-config flush stopped before saving a later manager");

        failing.edit().set("afterFailure", true);
        std::filesystem::create_directories(missingDirectory);
        require(ConfigManager::flushAll(), "all-config flush retry failed");
        const json recovered = readFile(failingPath);
        require(recovered.value("value", 0) == 41 &&
                    recovered.value("afterFailure", false),
                "failed flush froze the manager or lost its later edit");

        require(failing.shutdown(), "recovered manager shutdown failed");
        require(valid.shutdown(), "valid manager shutdown failed");
    }
}

int main() {
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() /
        ("sdrpp-config-tests-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root);

    try {
        testSetPathBeforeLoad(root / "not-loaded.json");
        testAccessAndLifetime(root / "access.json");
        testCrossManagerNestingRejected();
        testConcurrentSave(root / "concurrent.json");
        testShutdownLifecycle(root / "shutdown.json");
        testShutdownRetry(root / "missing-shutdown-directory");
#ifdef _WIN32
        testShutdownRetriesTransientWindowsLock(root / "shutdown-locked.json");
        testShutdownDoesNotRetryReadOnlyDestination(root / "shutdown-read-only.json");
#endif
#if OPT_CONFIG_MULTIPLE_INSTANCES
        testIndependentManagerMerge(root / "merge.json");
        testConcurrentShutdownMerge(root / "concurrent-shutdown.json");
        testCleanStaleShutdown(root / "clean-stale-shutdown.json");
#endif
        testSharedAutoSaver(root / "autosave-first.json", root / "autosave-second.json");
        testFlushAll(root / "flush-first.json", root / "flush-second.json");
        testFlushAllContinuesAfterFailure(
            root / "missing-flush-directory", root / "flush-valid.json");
        std::filesystem::remove_all(root);
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "config_manager_tests: " << e.what() << '\n';
        std::error_code ignored;
        std::filesystem::remove_all(root, ignored);
        return 1;
    }
}
