#include <gui/widgets/bandplan.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace {

void expect(bool condition, const char* message) {
    if (!condition) { throw std::runtime_error(message); }
}

class TemporaryDirectory {
public:
    TemporaryDirectory() {
        const auto nonce = std::chrono::high_resolution_clock::now()
            .time_since_epoch().count();
        path = std::filesystem::temp_directory_path() /
            ("sdrpp-bandplan-load-tests-" + std::to_string(nonce));
        std::filesystem::create_directories(path);
    }

    ~TemporaryDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path, error);
    }

    std::filesystem::path path;
};

void writeFile(const std::filesystem::path& path, const char* contents) {
    std::ofstream file(path);
    if (!file) { throw std::runtime_error("could not create band-plan fixture"); }
    file << contents;
}

void testMalformedPlansDoNotBlockValidPlans() {
    TemporaryDirectory directory;
    writeFile(directory.path / "malformed.json", "{");
    writeFile(directory.path / "wrong-type.json", R"json(
        {
          "name": "Wrong type",
          "country_name": "",
          "country_code": "",
          "author_name": "",
          "author_url": "",
          "bands": [
            { "name": "Bad", "type": "Band", "start": 1000, "end": 2000,
              "def_mode": 4 }
          ]
        })json");
    writeFile(directory.path / "valid.json", R"json(
        {
          "name": "Valid plan",
          "country_name": "",
          "country_code": "",
          "author_name": "",
          "author_url": "",
          "bands": [
            { "name": "Test band", "type": "Band", "start": 1000, "end": 2000,
              "def_mode": "FT8" }
          ]
        })json");

    bandplan::loadFromDir(directory.path.generic_string());

    expect(bandplan::bandplans.size() == 1,
           "malformed band plans were loaded or blocked the valid plan");
    const auto valid = bandplan::bandplans.find("Valid plan");
    expect(valid != bandplan::bandplans.end(), "valid band plan was not loaded");
    expect(valid->second.bands.size() == 1, "valid band row was not loaded");
    expect(valid->second.bands[0].defMode == "FT8",
           "unknown def_mode was not preserved for runtime fallback");
}

}

int main() {
    testMalformedPlansDoNotBlockValidPlans();
    return 0;
}
