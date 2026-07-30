#pragma once

#include <frequency_catalog/catalog.h>

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace station_schedules {

    struct UtcDate {
        int year = 0;
        int month = 0;
        int day = 0;
    };

    // Opaque provider scope plus its authoritative download location. Updaters
    // iterate targets in order, applying HTTP cache validators independently.
    struct SourceTarget {
        std::string scopeKey;
        std::string sourceUrl;
    };

    struct ScheduleParseReport {
        size_t linesRead = 0;
        size_t headerLines = 0;
        size_t recordsParsed = 0;
        size_t malformedLines = 0;
        size_t duplicateRecords = 0;
        size_t unknownDayExpressions = 0;
    };

    // Parsing is deliberately separate from network and cache policy. A
    // worker downloads bytes, asks a source to normalize them, validates the
    // resulting ProviderSnapshot, then atomically replaces the processed
    // ProviderCacheStore entry.
    class ScheduleSource {
    public:
        virtual ~ScheduleSource() = default;

        virtual const char* providerName() const = 0;
        virtual std::vector<SourceTarget> targetsFor(const UtcDate& date) const = 0;
        virtual bool parse(
            std::string_view payload,
            const SourceTarget& target,
            frequency_catalog::ProviderSnapshot& snapshot,
            ScheduleParseReport& report,
            std::string& error) const = 0;
    };

}
