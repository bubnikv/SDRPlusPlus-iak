#include "test_support.h"

#include <gui/colormaps.h>

namespace {

using test_support::TemporaryDirectory;
using test_support::expect;
using test_support::writeFile;

// Loads one fixture into a clean table and reports whether it was accepted.
// Nothing here may throw: loadMap() runs over a directory of hand-editable
// files during startup, with no guard at the call site.
bool loadFixture(const char* contents) {
    TemporaryDirectory directory("sdrpp-colormap-load-tests-");
    const auto path = directory.path / "map.json";
    writeFile(path, contents);

    colormaps::maps.clear();
    colormaps::loadMap(path.generic_string());
    return !colormaps::maps.empty();
}

void testValidMapLoads() {
    expect(loadFixture(R"json(
        {
          "name": "Test map",
          "author": "Tests",
          "map": ["#000000", "#0080FF", "#ffffff"]
        })json"),
        "a valid color map was rejected");

    const auto map = colormaps::maps.find("Test map");
    expect(map != colormaps::maps.end(), "the map was filed under the wrong name");
    expect(map->second.author == "Tests", "the author was not loaded");
    expect(map->second.entryCount() == 3, "the wrong number of entries was loaded");
    expect(!map->second.colors.empty(), "a loaded map has no colors");

    const float* colors = map->second.colors.data();
    expect(colors[0] == 0.0f && colors[1] == 0.0f && colors[2] == 0.0f,
           "the first color was decoded incorrectly");
    expect(colors[3] == 0.0f && colors[4] == 128.0f && colors[5] == 255.0f,
           "the second color was decoded incorrectly");
    expect(colors[6] == 255.0f && colors[7] == 255.0f && colors[8] == 255.0f,
           "lowercase hex was decoded incorrectly");
}

void testMalformedMapsAreRejected() {
    // Each of these reached an uncaught throw before: the truncated document
    // threw out of operator>>, which sat outside the guard, and the short and
    // non-hex literals reached std::stoi("") / std::stoi("GG") below it.
    struct Fixture {
        const char* what;
        const char* contents;
    };
    const Fixture fixtures[] = {
        { "a truncated document", "{" },
        { "an empty file", "" },
        { "a non-object document", R"json(["#000000"])json" },
        { "a missing name", R"json({ "author": "T", "map": ["#000000"] })json" },
        { "a missing map", R"json({ "name": "N", "author": "T" })json" },
        { "a non-string name",
          R"json({ "name": 5, "author": "T", "map": ["#000000"] })json" },
        { "a non-array map",
          R"json({ "name": "N", "author": "T", "map": "#000000" })json" },
        { "a non-string color",
          R"json({ "name": "N", "author": "T", "map": [5] })json" },
        { "an empty map", R"json({ "name": "N", "author": "T", "map": [] })json" },
        { "an empty color",
          R"json({ "name": "N", "author": "T", "map": [""] })json" },
        { "a hash-only color",
          R"json({ "name": "N", "author": "T", "map": ["#"] })json" },
        { "a short color",
          R"json({ "name": "N", "author": "T", "map": ["#FF"] })json" },
        { "an RGBA color",
          R"json({ "name": "N", "author": "T", "map": ["#FF0000FF"] })json" },
        { "a non-hex color",
          R"json({ "name": "N", "author": "T", "map": ["#GGGGGG"] })json" },
        { "an unprefixed color",
          R"json({ "name": "N", "author": "T", "map": ["0000000"] })json" },
    };

    for (const Fixture& fixture : fixtures) {
        if (loadFixture(fixture.contents)) {
            throw std::runtime_error(
                std::string("a color map with ") + fixture.what + " was loaded");
        }
    }
}

void testPartialMapIsNotCommitted() {
    // The colors are decoded into a temporary and only published once every
    // entry has passed, so a map that goes bad halfway is not half-loaded.
    expect(!loadFixture(R"json(
        {
          "name": "Half good",
          "author": "Tests",
          "map": ["#000000", "#FF", "#FFFFFF"]
        })json"),
        "a color map with one bad entry was partially loaded");
}

void testMissingFileIsReported() {
    TemporaryDirectory directory("sdrpp-colormap-load-tests-");
    colormaps::maps.clear();
    colormaps::loadMap((directory.path / "absent.json").generic_string());
    expect(colormaps::maps.empty(), "a missing color map file was loaded");
}

void testDefaultConstructedMapIsEmpty() {
    const colormaps::Map map;
    expect(map.colors.empty(), "a default-constructed map contains colors");
    expect(map.entryCount() == 0, "a default-constructed map has a nonzero size");
}

void testDuplicateNameIsReplaced() {
    TemporaryDirectory directory("sdrpp-colormap-load-tests-");
    const auto firstPath = directory.path / "first.json";
    const auto invalidPath = directory.path / "invalid.json";
    const auto secondPath = directory.path / "second.json";
    writeFile(firstPath, R"json(
        { "name": "Duplicate", "author": "First", "map": ["#000000"] }
    )json");
    writeFile(invalidPath, R"json(
        { "name": "Duplicate", "author": "Broken", "map": ["#FF"] }
    )json");
    writeFile(secondPath, R"json(
        { "name": "Duplicate", "author": "Second", "map": ["#FFFFFF"] }
    )json");

    colormaps::maps.clear();
    colormaps::loadMap(firstPath.generic_string());
    colormaps::loadMap(invalidPath.generic_string());
    expect(colormaps::maps.at("Duplicate").author == "First",
           "an invalid replacement erased the existing map");
    colormaps::loadMap(secondPath.generic_string());

    expect(colormaps::maps.size() == 1, "a duplicate name created two maps");
    const colormaps::Map& map = colormaps::maps.at("Duplicate");
    expect(map.author == "Second", "a duplicate name did not replace the old map");
    expect(map.colors.size() == 3 &&
           map.colors[0] == 255.0f &&
           map.colors[1] == 255.0f &&
           map.colors[2] == 255.0f,
           "the replacement map has the wrong colors");
}

}

int main() {
    return test_support::run("colormap_load_tests", [] {
        testValidMapLoads();
        testMalformedMapsAreRejected();
        testPartialMapIsNotCommitted();
        testMissingFileIsReported();
        testDefaultConstructedMapIsEmpty();
        testDuplicateNameIsReplaced();
    });
}
