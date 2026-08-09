#pragma once
// Shared helpers for the core unit tests that load resources from disk.
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace test_support {

    inline void expect(bool condition, const char* message) {
        if (!condition) { throw std::runtime_error(message); }
    }

    // A scratch directory for fixtures, removed when the test returns. The name
    // carries a clock nonce so concurrent test executables never collide.
    class TemporaryDirectory {
    public:
        explicit TemporaryDirectory(const std::string& prefix) {
            const auto nonce = std::chrono::high_resolution_clock::now()
                .time_since_epoch().count();
            path = std::filesystem::temp_directory_path() /
                (prefix + std::to_string(nonce));
            std::filesystem::create_directories(path);
        }

        ~TemporaryDirectory() {
            std::error_code error;
            std::filesystem::remove_all(path, error);
        }

        TemporaryDirectory(const TemporaryDirectory&) = delete;
        TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;

        std::filesystem::path path;
    };

    inline void writeFile(const std::filesystem::path& path, const char* contents) {
        std::ofstream file(path);
        if (!file) { throw std::runtime_error("could not create test fixture"); }
        file << contents;
    }

    // Runs a suite, turning a failed expectation into a diagnostic and a
    // process exit code. Every one of these suites asserts that loading a
    // malformed resource does not throw, so an escaping exception is itself a
    // failure and must be reported rather than terminating the process.
    inline int run(const char* suite, void (*body)()) {
        try {
            body();
            return 0;
        }
        catch (const std::exception& e) {
            std::cerr << suite << ": " << e.what() << '\n';
            return 1;
        }
        catch (...) {
            std::cerr << suite << ": unknown error\n";
            return 1;
        }
    }

}
