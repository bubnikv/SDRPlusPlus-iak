#include "bookmark.h"
#include <radio_interface.h>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>

namespace {

void expect(bool condition, const char* message) {
    if (!condition) { throw std::runtime_error(message); }
}

void testCanonicalNamesRoundTrip() {
    for (int mode = 0; mode < _RADIO_IFACE_MODE_COUNT; mode++) {
        FrequencyBookmark bookmark{};
        bookmark.mode = mode;
        const json stored = bookmarkToJson(bookmark);
        expect(stored.at("mode") == radioModeName(mode), "canonical mode name missing");
        expect(bookmarkFromJson(stored).mode == mode, "canonical mode did not round-trip");
    }
}

void testAliases() {
    expect(bookmarkFromJson({ { "mode", "FM" } }).mode == RADIO_IFACE_MODE_NFM,
           "FM alias did not parse");
    expect(bookmarkFromJson({ { "mode", "CW-R" } }).mode == RADIO_IFACE_MODE_CWR,
           "CW-R alias did not parse");
    expect(bookmarkFromJson({ { "mode", "usb" } }).mode == RADIO_IFACE_MODE_USB,
           "lower-case canonical name did not parse");
}

void testNumericModesAreNotMigrated() {
    for (int mode = 0; mode < _RADIO_IFACE_MODE_COUNT; mode++) {
        expect(bookmarkFromJson({ { "mode", mode } }).mode == -1,
               "numeric mode was unexpectedly migrated");
    }
}

void testExternalFormats() {
    for (int mode = 0; mode < _RADIO_IFACE_MODE_COUNT; mode++) {
        const ExternalBookmarkParseResult numeric =
            bookmarkFromExternalJson({ { "mode", mode } });
        expect(numeric.bookmark.mode == mode, "external numeric mode did not parse");
        expect(numeric.modeStatus == ExternalBookmarkModeStatus::Resolved,
               "external numeric mode was not marked resolved");

        const ExternalBookmarkParseResult named =
            bookmarkFromExternalJson({ { "mode", radioModeName(mode) } });
        expect(named.bookmark.mode == mode, "external string mode did not parse");
        expect(named.modeStatus == ExternalBookmarkModeStatus::Resolved,
               "external string mode was not marked resolved");
    }

    const ExternalBookmarkParseResult missing = bookmarkFromExternalJson({});
    expect(missing.bookmark.mode == -1, "missing external mode was recorded");
    expect(missing.modeStatus == ExternalBookmarkModeStatus::Unrecorded,
           "missing external mode was not marked unrecorded");

    for (const json& invalid : {
             json("future-mode"),
             json(1.5),
             json(std::numeric_limits<std::int64_t>::max()),
             json(std::numeric_limits<std::uint64_t>::max()) })
    {
        const ExternalBookmarkParseResult parsed =
            bookmarkFromExternalJson({ { "mode", invalid } });
        expect(parsed.bookmark.mode == -1, "invalid external mode was recorded");
        expect(parsed.modeStatus == ExternalBookmarkModeStatus::Invalid,
               "invalid external mode was not reported");
    }
}

void testUnrecordedMode() {
    FrequencyBookmark bookmark{};
    bookmark.mode = -1;
    const json stored = bookmarkToJson(bookmark);
    expect(!stored.contains("mode"), "unrecorded mode emitted a name");
    expect(bookmarkFromJson(stored).mode == -1, "unrecorded mode did not round-trip");
}

}

int main() {
    testCanonicalNamesRoundTrip();
    testAliases();
    testNumericModesAreNotMigrated();
    testExternalFormats();
    testUnrecordedMode();
    return 0;
}
