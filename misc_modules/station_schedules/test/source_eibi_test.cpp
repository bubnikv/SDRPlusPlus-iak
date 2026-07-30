/*
 * Parser fixture smoke test. The CSV excerpt was downloaded from
 * http://www.eibispace.de/dx/sked-a26.csv on 2026-07-28. It intentionally
 * combines the beginning of that file with representative real rows for
 * day ranges, calendar expressions, validity dates, sideband markers, and
 * an overnight schedule. It is not wired into the SDR++ build.
 */

#include "../src/source_eibi.h"

#include <cassert>
#include <fstream>
#include <iterator>
#include <string>

using frequency_catalog::ProviderSnapshot;
using station_schedules::EibiSource;
using station_schedules::ScheduleParseReport;
using station_schedules::SourceTarget;
using station_schedules::UtcDate;

namespace {

    ProviderSnapshot parseOne(const std::string& row, ScheduleParseReport& report) {
        EibiSource source;
        ProviderSnapshot snapshot;
        std::string error;
        bool ok = source.parse(row, {
            "sked-a26",
            "http://www.eibispace.de/dx/sked-a26.csv"
        }, snapshot, report, error);
        assert(ok);
        assert(error.empty());
        assert(snapshot.eibiSchedules.size() == 1);
        return snapshot;
    }

}

int main(int argc, char** argv) {
    EibiSource source;

    assert(station_schedules::eibiSeasonFor({ 2026, 3, 28 }).key() == "b25");
    assert(station_schedules::eibiSeasonFor({ 2026, 3, 29 }).key() == "a26");
    assert(station_schedules::eibiSeasonFor({ 2026, 10, 24 }).key() == "a26");
    assert(station_schedules::eibiSeasonFor({ 2026, 10, 25 }).key() == "b26");
    assert(source.targetsFor(UtcDate{ 2026, 7, 28 })[1].scopeKey == "sked-b25");

    ScheduleParseReport report;
    ProviderSnapshot daily = parseOne(
        "1000;0000-2400;;G;Daily;E;Eu;wo;1;;\n", report);
    assert(daily.eibiSchedules[0].schedule.dayMask == 0x7F);
    assert(daily.eibiSchedules[0].schedule.endMinuteUtc == 1440);
    assert(daily.eibiSchedules[0].transmitterSite == "G-wo");

    ProviderSnapshot weekdays = parseOne(
        "1001;2200-0100;Mo-Fr;G;Weekdays;E;Eu;;1;;\n", report);
    assert(weekdays.eibiSchedules[0].schedule.dayMask == 0x3E);
    assert(weekdays.eibiSchedules[0].schedule.startMinuteUtc == 22 * 60);
    assert(weekdays.eibiSchedules[0].schedule.endMinuteUtc == 60);

    ProviderSnapshot digits = parseOne(
        "1002;1200-1300;1245;G;Digits;E;Eu;;1;;\n", report);
    assert(digits.eibiSchedules[0].schedule.dayMask == 0x36);

    ProviderSnapshot unknown = parseOne(
        "1003;1200-1300;1.Sa;G;Monthly;E;Eu;;1;;\n", report);
    assert(unknown.eibiSchedules[0].schedule.dayMask == 0x7F);
    assert(report.unknownDayExpressions == 1);

    ProviderSnapshot dated = parseOne(
        "1004;1200-1300;;G;Dated;E;Eu;;6;2903;3006[1125]\n", report);
    assert(dated.eibiSchedules[0].schedule.validFromYmd == 20260329);
    assert(dated.eibiSchedules[0].schedule.validUntilYmd == 20260630);
    assert(dated.eibiSchedules[0].stopDateText == "3006[1125]");

    std::string cp1252 =
        "1005;1200-1300;;B;R";
    cp1252 += "\xE1";
    cp1252 += "dio Clube do Par";
    cp1252 += "\xE1";
    cp1252 += ";P;B;be;1;;\n";
    ProviderSnapshot converted = parseOne(cp1252, report);
    std::string expectedUtf8 = "R";
    expectedUtf8 += "\xC3\xA1";
    expectedUtf8 += "dio Clube do Par";
    expectedUtf8 += "\xC3\xA1";
    assert(converted.eibiSchedules[0].station == expectedUtf8);

    ProviderSnapshot first = parseOne(
        "1006;1200-1300;;G;Stable;E;Eu;;1;;\n", report);
    ProviderSnapshot second = parseOne(
        "1006;1200-1300;;G;Stable;E;Eu;;1;;\n", report);
    assert(first.eibiSchedules[0].sourceRef.recordId
        == second.eibiSchedules[0].sourceRef.recordId);

    ProviderSnapshot garbage;
    std::string error;
    bool ok = source.parse("garbage\ntruncated;row\n", {
        "sked-a26",
        "http://www.eibispace.de/dx/sked-a26.csv"
    }, garbage, report, error);
    assert(!ok);
    assert(!error.empty());

    if (argc == 2) {
        std::ifstream input(argv[1], std::ios::binary);
        assert(input.good());
        std::string sample{
            std::istreambuf_iterator<char>(input),
            std::istreambuf_iterator<char>()
        };
        ProviderSnapshot fixture;
        ok = source.parse(sample, {
            "sked-a26",
            "http://www.eibispace.de/dx/sked-a26.csv"
        }, fixture, report, error);
        assert(ok);
        assert(fixture.eibiSchedules.size() == 51);
        assert(report.headerLines == 1);
    }

    return 0;
}
