/*
 * Concept inspired by shortwave-station-list-sdrpp by Otto Pattemore
 * (GPL-3.0). Schedule data from EiBi (http://www.eibispace.de) by
 * Eike Bierwirth.
 */

#include "source_eibi.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace station_schedules {
    namespace {

        constexpr const char* EIBI_URL_PREFIX = "http://www.eibispace.de/dx/sked-";

        // Verified against sked-a26.csv on 2026-07-28 and EiBi README.TXT.
        // The file is CP1252, contains one header row, and every data row has
        // exactly ten semicolons:
        //   0 kHz, 1 HHMM-HHMM, 2 days, 3 ITU, 4 station, 5 language,
        //   6 target, 7 transmitter site, 8 persistence, 9 start, 10 stop.

        int dayOfWeek(int year, int month, int day) {
            static constexpr int OFFSETS[] = {
                0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4
            };
            if (month < 3) {
                year--;
            }
            return (year + year / 4 - year / 100 + year / 400
                + OFFSETS[month - 1] + day) % 7;
        }

        bool isLeapYear(int year) {
            return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
        }

        int daysInMonth(int year, int month) {
            static constexpr int DAYS[] = {
                31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
            };
            if (month == 2 && isLeapYear(year)) {
                return 29;
            }
            return DAYS[month - 1];
        }

        int lastSunday(int year, int month) {
            int lastDay = daysInMonth(year, month);
            return lastDay - dayOfWeek(year, month, lastDay);
        }

        std::string_view trimAscii(std::string_view value) {
            while (!value.empty()
                && (value.front() == ' ' || value.front() == '\t')) {
                value.remove_prefix(1);
            }
            while (!value.empty()
                && (value.back() == ' ' || value.back() == '\t'
                    || value.back() == '\r')) {
                value.remove_suffix(1);
            }
            return value;
        }

        void appendUtf8(std::string& result, uint32_t codePoint) {
            if (codePoint <= 0x7F) {
                result.push_back(static_cast<char>(codePoint));
            }
            else if (codePoint <= 0x7FF) {
                result.push_back(static_cast<char>(0xC0 | (codePoint >> 6)));
                result.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
            }
            else {
                result.push_back(static_cast<char>(0xE0 | (codePoint >> 12)));
                result.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
                result.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
            }
        }

        std::string cp1252ToUtf8(std::string_view value) {
            static constexpr std::array<uint16_t, 32> CP1252_HIGH = {
                0x20AC, 0xFFFD, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021,
                0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0xFFFD, 0x017D, 0xFFFD,
                0xFFFD, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
                0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0xFFFD, 0x017E, 0x0178
            };

            std::string result;
            result.reserve(value.size() + value.size() / 4);
            for (unsigned char c : value) {
                uint32_t codePoint = c;
                if (c >= 0x80 && c <= 0x9F) {
                    codePoint = CP1252_HIGH[c - 0x80];
                }
                appendUtf8(result, codePoint);
            }
            return result;
        }

        std::vector<std::string_view> splitRow(std::string_view line) {
            std::vector<std::string_view> columns;
            size_t begin = 0;
            for (;;) {
                size_t separator = line.find(';', begin);
                if (separator == std::string_view::npos) {
                    columns.push_back(trimAscii(line.substr(begin)));
                    break;
                }
                columns.push_back(trimAscii(line.substr(begin, separator - begin)));
                begin = separator + 1;
            }
            return columns;
        }

        bool parseUnsigned(std::string_view value, int& result) {
            if (value.empty()) {
                return false;
            }
            int parsed = 0;
            for (char c : value) {
                if (c < '0' || c > '9') {
                    return false;
                }
                parsed = parsed * 10 + (c - '0');
                if (parsed > 1000000) {
                    return false;
                }
            }
            result = parsed;
            return true;
        }

        bool parseFrequency(std::string_view value, double& frequencyHz) {
            if (value.empty()) {
                return false;
            }
            double frequencyKhz = 0.0;
            double fraction = 0.1;
            bool seenDecimalPoint = false;
            bool seenDigit = false;
            for (char c : value) {
                if (c == '.' && !seenDecimalPoint) {
                    seenDecimalPoint = true;
                    continue;
                }
                if (c < '0' || c > '9') {
                    return false;
                }
                seenDigit = true;
                if (!seenDecimalPoint) {
                    frequencyKhz = frequencyKhz * 10.0 + (c - '0');
                }
                else {
                    frequencyKhz += (c - '0') * fraction;
                    fraction *= 0.1;
                }
            }
            if (!seenDigit || frequencyKhz <= 0.0) {
                return false;
            }
            frequencyHz = std::round(frequencyKhz * 1000.0);
            return std::isfinite(frequencyHz) && frequencyHz > 0.0;
        }

        bool parseTime(std::string_view value, bool allowEndOfDay, int& minutes) {
            int hhmm = 0;
            if (value.size() != 4 || !parseUnsigned(value, hhmm)) {
                return false;
            }
            if (allowEndOfDay && hhmm == 2400) {
                minutes = 1440;
                return true;
            }
            int hour = hhmm / 100;
            int minute = hhmm % 100;
            if (hour > 23 || minute > 59) {
                return false;
            }
            minutes = hour * 60 + minute;
            return true;
        }

        bool parseTimeRange(
            std::string_view value,
            int& startMinute,
            int& endMinute) {
            return value.size() == 9 && value[4] == '-'
                && parseTime(value.substr(0, 4), false, startMinute)
                && parseTime(value.substr(5, 4), true, endMinute);
        }

        int dayBit(std::string_view value) {
            if (value == "Su") { return 0; }
            if (value == "Mo") { return 1; }
            if (value == "Tu") { return 2; }
            if (value == "We") { return 3; }
            if (value == "Th") { return 4; }
            if (value == "Fr") { return 5; }
            if (value == "Sa") { return 6; }
            return -1;
        }

        bool parseDayToken(std::string_view value, uint8_t& dayMask) {
            if (value.size() == 5 && value[2] == '-') {
                int first = dayBit(value.substr(0, 2));
                int last = dayBit(value.substr(3, 2));
                if (first < 0 || last < 0) {
                    return false;
                }
                for (int day = first;; day = (day + 1) % 7) {
                    dayMask |= static_cast<uint8_t>(1U << day);
                    if (day == last) {
                        break;
                    }
                }
                return true;
            }

            if (value.empty() || value.size() % 2 != 0) {
                return false;
            }
            for (size_t i = 0; i < value.size(); i += 2) {
                int day = dayBit(value.substr(i, 2));
                if (day < 0) {
                    return false;
                }
                dayMask |= static_cast<uint8_t>(1U << day);
            }
            return true;
        }

        uint8_t parseDays(std::string_view value, bool& recognized) {
            if (value.empty() || value == "USB" || value == "LSB") {
                recognized = true;
                return 0x7F;
            }

            uint8_t dayMask = 0;
            bool digitsOnly = true;
            for (char c : value) {
                if (c < '1' || c > '7') {
                    digitsOnly = false;
                    break;
                }
                int day = c == '7' ? 0 : c - '0';
                dayMask |= static_cast<uint8_t>(1U << day);
            }
            if (digitsOnly) {
                recognized = true;
                return dayMask;
            }

            dayMask = 0;
            size_t begin = 0;
            for (;;) {
                size_t separator = value.find(',', begin);
                std::string_view token = separator == std::string_view::npos
                    ? value.substr(begin)
                    : value.substr(begin, separator - begin);
                if (!parseDayToken(token, dayMask)) {
                    recognized = false;
                    return 0x7F;
                }
                if (separator == std::string_view::npos) {
                    break;
                }
                begin = separator + 1;
            }
            recognized = true;
            return dayMask;
        }

        std::string lowercaseAscii(std::string_view value) {
            std::string result(value);
            for (char& c : result) {
                if (c >= 'A' && c <= 'Z') {
                    c = static_cast<char>(c - 'A' + 'a');
                }
            }
            return result;
        }

        bool contains(const std::string& value, const char* needle) {
            return value.find(needle) != std::string::npos;
        }

        struct Tuning {
            int mode = RADIO_IFACE_MODE_AM;
            double offsetHz = 0.0;
        };

        Tuning inferTuning(
            double carrierFrequency,
            const std::string& station,
            const std::string& language,
            const std::string& daysText) {
            std::string name = lowercaseAscii(station);
            if (language == "-CW") {
                return { RADIO_IFACE_MODE_CW, 0.0 };
            }
            if (daysText == "LSB") {
                return { RADIO_IFACE_MODE_LSB, 0.0 };
            }
            if (daysText == "USB") {
                return { RADIO_IFACE_MODE_USB, 0.0 };
            }
            if (language == "-TY" || contains(name, "rtty") || contains(name, "fsk")) {
                return { RADIO_IFACE_MODE_USB, -1000.0 };
            }
            if (contains(name, " fax")) {
                return { RADIO_IFACE_MODE_USB, -1900.0 };
            }
            if (language == "-HF" || contains(name, "hfdl")) {
                return { RADIO_IFACE_MODE_USB, -1000.0 };
            }
            // OpenWebRX+ treats every entry below 4.8 MHz as USB. EiBi also
            // contains ordinary medium-wave and tropical-band broadcasters,
            // so use that fallback only below the MW broadcast band.
            if (contains(name, "volmet") || contains(name, "cross ")
                || contains(name, " ldoc") || contains(name, " car-")
                || contains(name, " nat-") || contains(name, " usb")
                || carrierFrequency < 500000.0) {
                return { RADIO_IFACE_MODE_USB, 0.0 };
            }
            return { RADIO_IFACE_MODE_AM, 0.0 };
        }

        std::string normalizeSite(
            const std::string& countryCode,
            const std::string& siteCode) {
            if (siteCode.empty()) {
                return countryCode;
            }
            if (siteCode.front() == '/') {
                return siteCode.substr(1);
            }
            return countryCode + "-" + siteCode;
        }

        bool parseSeason(const SourceTarget& target, EibiSeason& season) {
            if (target.scopeKey.size() != 8
                || target.scopeKey.compare(0, 5, "sked-") != 0
                || (target.scopeKey[5] != 'a' && target.scopeKey[5] != 'b')
                || target.scopeKey[6] < '0' || target.scopeKey[6] > '9'
                || target.scopeKey[7] < '0' || target.scopeKey[7] > '9') {
                return false;
            }
            season.schedule = target.scopeKey[5];
            season.year = 2000 + (target.scopeKey[6] - '0') * 10
                + (target.scopeKey[7] - '0');
            return true;
        }

        int parseDate(
            const std::string& text,
            const EibiSeason& season) {
            // EiBi may append a bracketed last-heard date to a validity date,
            // for example "3006[1125]". A value consisting only of brackets
            // is historical metadata and does not constrain validity.
            size_t bracket = text.find('[');
            std::string_view dateText(text.data(),
                bracket == std::string::npos ? text.size() : bracket);
            if (dateText.size() != 4) {
                return 0;
            }
            int dayMonth = 0;
            if (!parseUnsigned(dateText, dayMonth)) {
                return 0;
            }
            int day = dayMonth / 100;
            int month = dayMonth % 100;
            if (month < 1 || month > 12) {
                return 0;
            }
            int year = season.year;
            if (season.schedule == 'b' && month <= 3) {
                year++;
            }
            if (day < 1 || day > daysInMonth(year, month)) {
                return 0;
            }
            return year * 10000 + month * 100 + day;
        }

        std::string integerKey(double value) {
            return std::to_string(static_cast<long long>(std::llround(value)));
        }

    }

    std::string EibiSeason::key() const {
        int shortYear = year % 100;
        std::string result;
        result.reserve(3);
        result.push_back(schedule);
        result.push_back(static_cast<char>('0' + shortYear / 10));
        result.push_back(static_cast<char>('0' + shortYear % 10));
        return result;
    }

    EibiSeason eibiSeasonFor(const UtcDate& date) {
        int aStart = lastSunday(date.year, 3);
        int bStart = lastSunday(date.year, 10);
        if (date.month < 3 || (date.month == 3 && date.day < aStart)) {
            return { 'b', date.year - 1 };
        }
        if (date.month > 10 || (date.month == 10 && date.day >= bStart)) {
            return { 'b', date.year };
        }
        return { 'a', date.year };
    }

    EibiSeason previousEibiSeason(const EibiSeason& season) {
        return season.schedule == 'a'
            ? EibiSeason{ 'b', season.year - 1 }
            : EibiSeason{ 'a', season.year };
    }

    const char* EibiSource::providerName() const {
        return "eibi";
    }

    std::vector<SourceTarget> EibiSource::targetsFor(const UtcDate& date) const {
        EibiSeason current = eibiSeasonFor(date);
        EibiSeason previous = previousEibiSeason(current);
        std::vector<SourceTarget> result;
        result.reserve(2);
        for (const EibiSeason& season : { current, previous }) {
            std::string key = season.key();
            result.push_back({
                "sked-" + key,
                std::string(EIBI_URL_PREFIX) + key + ".csv"
            });
        }
        return result;
    }

    bool EibiSource::parse(
        std::string_view payload,
        const SourceTarget& target,
        frequency_catalog::ProviderSnapshot& snapshot,
        ScheduleParseReport& report,
        std::string& error) const {
        EibiSeason season;
        if (!parseSeason(target, season)) {
            error = "invalid EiBi scope key: " + target.scopeKey;
            return false;
        }

        frequency_catalog::ProviderSnapshot parsed;
        ScheduleParseReport parsedReport;
        std::unordered_set<std::string> recordIds;

        size_t begin = 0;
        while (begin <= payload.size()) {
            size_t newline = payload.find('\n', begin);
            std::string_view line = newline == std::string_view::npos
                ? payload.substr(begin)
                : payload.substr(begin, newline - begin);
            line = trimAscii(line);
            if (!line.empty()) {
                parsedReport.linesRead++;
                std::vector<std::string_view> columns = splitRow(line);
                double carrierFrequency = 0.0;
                if (!columns.empty() && columns[0].substr(0, 3) == "kHz") {
                    // The current header ends in an extra semicolon, unlike
                    // data rows, so recognize it before enforcing 11 fields.
                    parsedReport.headerLines++;
                }
                else if (columns.size() == 11 && !columns[4].empty()
                    && parseFrequency(columns[0], carrierFrequency)) {
                    int startMinute = 0;
                    int endMinute = 0;
                    int persistenceCode = 0;
                    if (!parseTimeRange(columns[1], startMinute, endMinute)
                        || !parseUnsigned(columns[8], persistenceCode)
                        || persistenceCode > 99) {
                        parsedReport.malformedLines++;
                    }
                    else {
                        frequency_catalog::EibiScheduleRecord record;
                        record.station = cp1252ToUtf8(columns[4]);
                        record.countryCode = cp1252ToUtf8(columns[3]);
                        record.language = cp1252ToUtf8(columns[5]);
                        record.target = cp1252ToUtf8(columns[6]);
                        record.transmitterSiteCode = cp1252ToUtf8(columns[7]);
                        record.transmitterSite = normalizeSite(
                            record.countryCode, record.transmitterSiteCode);
                        record.persistenceCode = persistenceCode;
                        record.startDateText = cp1252ToUtf8(columns[9]);
                        record.stopDateText = cp1252ToUtf8(columns[10]);
                        record.carrierFrequency = carrierFrequency;

                        std::string daysText = cp1252ToUtf8(columns[2]);
                        bool recognizedDays = false;
                        record.schedule.startMinuteUtc = startMinute;
                        record.schedule.endMinuteUtc = endMinute;
                        record.schedule.dayMask = parseDays(daysText, recognizedDays);
                        record.schedule.daysText = daysText;
                        if (!recognizedDays) {
                            parsedReport.unknownDayExpressions++;
                        }
                        if (persistenceCode == 6) {
                            record.schedule.validFromYmd =
                                parseDate(record.startDateText, season);
                            record.schedule.validUntilYmd =
                                parseDate(record.stopDateText, season);
                        }

                        Tuning tuning = inferTuning(
                            carrierFrequency, record.station,
                            record.language, daysText);
                        record.mode = tuning.mode;
                        record.tuningFrequency = carrierFrequency + tuning.offsetHz;
                        if (record.tuningFrequency <= 0.0) {
                            record.tuningFrequency = carrierFrequency;
                        }

                        std::string carrierKey = integerKey(carrierFrequency);
                        std::string startKey = std::to_string(startMinute);
                        std::string endKey = std::to_string(endMinute);
                        std::string persistenceKey = std::to_string(persistenceCode);
                        record.sourceRef.provider = providerName();
                        record.sourceRef.url = target.sourceUrl;
                        record.sourceRef.recordId =
                            frequency_catalog::makeProviderRecordId(providerName(), {
                                carrierKey,
                                startKey,
                                endKey,
                                daysText,
                                record.countryCode,
                                record.station,
                                record.language,
                                record.target,
                                record.transmitterSiteCode,
                                persistenceKey,
                                record.startDateText,
                                record.stopDateText
                            });

                        if (!recordIds.insert(record.sourceRef.recordId.str()).second) {
                            parsedReport.duplicateRecords++;
                        }
                        else {
                            parsed.eibiSchedules.push_back(std::move(record));
                            parsedReport.recordsParsed++;
                        }
                    }
                }
                else {
                    parsedReport.malformedLines++;
                }
            }
            if (newline == std::string_view::npos) {
                break;
            }
            begin = newline + 1;
        }

        if (parsed.eibiSchedules.empty()) {
            error = "EiBi payload contains no valid schedule records";
            report = parsedReport;
            return false;
        }

        std::sort(parsed.eibiSchedules.begin(), parsed.eibiSchedules.end(),
            [](const frequency_catalog::EibiScheduleRecord& a,
                const frequency_catalog::EibiScheduleRecord& b) {
                if (a.tuningFrequency != b.tuningFrequency) {
                    return a.tuningFrequency < b.tuningFrequency;
                }
                if (a.carrierFrequency != b.carrierFrequency) {
                    return a.carrierFrequency < b.carrierFrequency;
                }
                if (a.station != b.station) {
                    return a.station < b.station;
                }
                return a.sourceRef.recordId < b.sourceRef.recordId;
            });

        snapshot = std::move(parsed);
        report = parsedReport;
        error.clear();
        return true;
    }

}
