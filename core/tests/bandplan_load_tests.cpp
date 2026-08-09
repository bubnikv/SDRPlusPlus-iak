#include "test_support.h"

#include <gui/widgets/bandplan.h>

namespace {

using test_support::TemporaryDirectory;
using test_support::expect;
using test_support::writeFile;

void testMalformedPlansDoNotBlockValidPlans() {
    TemporaryDirectory directory("sdrpp-bandplan-load-tests-");
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

    bandplan::loadFromDir(directory.path.generic_string());
    expect(bandplan::bandplans.size() == 1 && bandplan::bandplanNames.size() == 1,
           "reloading duplicated the band-plan index");
    expect(bandplan::bandplanNames[0] == "Valid plan",
           "reloading left a stale band-plan name");
}

void testMalformedColorsDoNotThrow() {
    // Every one of these reached an uncaught throw before: a non-string hits
    // json::type_error, "#" and "#FF" reach std::stoi("").
    const json table = json::parse(R"json(
        {
          "amateur":   "#FF0000FF",
          "empty":     "",
          "hash-only": "#",
          "too-short": "#FF",
          "too-long":  "#FF0000FFFF",
          "not-hex":   "#GGGGGGGG",
          "number":    5,
          "object":    {}
        })json");

    bandplan::loadColorTable(table);

    expect(bandplan::colorTable.size() == 1,
           "malformed colors were accepted or rejected the valid one");
    const auto amateur = bandplan::colorTable.find("amateur");
    expect(amateur != bandplan::colorTable.end(), "valid color was not loaded");
    expect(amateur->second.colorValue == IM_COL32(0xFF, 0x00, 0x00, 0xFF),
           "valid color was parsed incorrectly");
    expect(amateur->second.transColorValue == IM_COL32(0xFF, 0x00, 0x00, 100),
           "valid transparent color was parsed incorrectly");
}

void testNonObjectColorTableIsIgnored() {
    bandplan::loadColorTable(json::parse(R"json(["#FF0000FF"])json"));
    expect(bandplan::colorTable.empty(),
           "a non-object color table left entries behind");
}

}

int main() {
    return test_support::run("bandplan_load_tests", [] {
        testMalformedPlansDoNotBlockValidPlans();
        testMalformedColorsDoNotThrow();
        testNonObjectColorTableIsIgnored();
    });
}
