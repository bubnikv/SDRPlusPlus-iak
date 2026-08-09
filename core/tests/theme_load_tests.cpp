#include "test_support.h"

#include <gui/theme_manager.h>

namespace {

using test_support::TemporaryDirectory;
using test_support::expect;
using test_support::writeFile;

const char* const VALID_THEME = R"json(
    {
      "name": "Test theme",
      "author": "Tests",
      "Text": "#FFFFFFFF",
      "Border": "#6D6D7F7F",
      "WaterfallBackground": "#000000FF",
      "ClearColor": "#111111FF",
      "FFTHoldColor": "#FFFF00FF"
    })json";

// Loads one fixture into a fresh manager and reports whether it was accepted.
// Nothing here may throw: loadThemesFromDir() walks a directory of
// hand-editable files during startup, with no guard at the call site.
bool loadFixture(const char* contents) {
    TemporaryDirectory directory("sdrpp-theme-load-tests-");
    const auto path = directory.path / "theme.json";
    writeFile(path, contents);

    ThemeManager manager;
    return manager.loadTheme(path.generic_string());
}

void testValidThemeLoads() {
    TemporaryDirectory directory("sdrpp-theme-load-tests-");
    const auto path = directory.path / "theme.json";
    writeFile(path, VALID_THEME);

    ThemeManager manager;
    expect(manager.loadTheme(path.generic_string()), "a valid theme was rejected");

    const std::vector<std::string> names = manager.getThemeNames();
    expect(names.size() == 1, "the wrong number of themes was loaded");
    expect(names[0] == "Test theme", "the theme was filed under the wrong name");

    // A theme name may only be claimed once.
    expect(!manager.loadTheme(path.generic_string()),
           "a duplicate theme name was accepted");
}

void testMalformedThemesAreRejected() {
    // The first two reached an uncaught throw before: the truncated document
    // threw out of operator>>, which sat above the guard, and the non-string
    // field threw out of the wholesale map<string, string> conversion.
    struct Fixture {
        const char* what;
        const char* contents;
    };
    const Fixture fixtures[] = {
        { "a truncated document", "{" },
        { "an empty file", "" },
        { "a non-string field",
          R"json({ "name": "N", "author": "T", "Text": 5 })json" },
        { "a nested object field",
          R"json({ "name": "N", "author": "T", "Text": { "r": 1 } })json" },
        { "a non-object document", R"json(["#FFFFFFFF"])json" },
        { "a missing name", R"json({ "author": "T" })json" },
        { "a non-string name", R"json({ "name": 5, "author": "T" })json" },
        { "a non-string author", R"json({ "name": "N", "author": 5 })json" },
        { "an unknown field",
          R"json({ "name": "N", "author": "T", "NotAColor": "#FFFFFFFF" })json" },
        { "an empty color",
          R"json({ "name": "N", "author": "T", "Text": "" })json" },
        { "a hash-only color",
          R"json({ "name": "N", "author": "T", "Text": "#" })json" },
        { "a short color",
          R"json({ "name": "N", "author": "T", "Text": "#FF" })json" },
        { "an RGB color",
          R"json({ "name": "N", "author": "T", "Text": "#FFFFFF" })json" },
        { "a non-hex color",
          R"json({ "name": "N", "author": "T", "Text": "#GGGGGGGG" })json" },
        { "an unprefixed color",
          R"json({ "name": "N", "author": "T", "Text": "FFFFFFFFF" })json" },
        { "a bad waterfall background",
          R"json({ "name": "N", "author": "T", "WaterfallBackground": "#FF" })json" },
        { "a bad clear color",
          R"json({ "name": "N", "author": "T", "ClearColor": "#FF" })json" },
        { "a bad FFT hold color",
          R"json({ "name": "N", "author": "T", "FFTHoldColor": "#FF" })json" },
    };

    for (const Fixture& fixture : fixtures) {
        if (loadFixture(fixture.contents)) {
            throw std::runtime_error(
                std::string("a theme with ") + fixture.what + " was loaded");
        }
    }
}

void testMissingFileIsReported() {
    TemporaryDirectory directory("sdrpp-theme-load-tests-");
    ThemeManager manager;
    expect(!manager.loadTheme((directory.path / "absent.json").generic_string()),
           "a missing theme file was loaded");
}

void testMalformedThemesDoNotBlockValidThemes() {
    TemporaryDirectory directory("sdrpp-theme-load-tests-");
    writeFile(directory.path / "malformed.json", "{");
    writeFile(directory.path / "wrong-type.json",
              R"json({ "name": "Wrong type", "author": "T", "Text": 5 })json");
    writeFile(directory.path / "notes.txt", "not a theme");
    writeFile(directory.path / "valid.json", VALID_THEME);

    ThemeManager manager;
    expect(manager.loadThemesFromDir(directory.path.generic_string()),
           "loading a directory of themes failed");

    const std::vector<std::string> names = manager.getThemeNames();
    expect(names.size() == 1,
           "malformed themes were loaded or blocked the valid theme");
    expect(names[0] == "Test theme", "the valid theme was not loaded");
}

}

int main() {
    return test_support::run("theme_load_tests", [] {
        testValidThemeLoads();
        testMalformedThemesAreRejected();
        testMissingFileIsReported();
        testMalformedThemesDoNotBlockValidThemes();
    });
}
