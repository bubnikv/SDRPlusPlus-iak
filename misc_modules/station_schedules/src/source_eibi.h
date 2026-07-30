#pragma once

#include "schedule_source.h"

#include <cstddef>
#include <string>

namespace station_schedules {

    // The current A26 CSV is about 0.49 MiB. Keep a 4x transfer guard before
    // parsing; the processed ProviderCacheStore has its own 16 MiB limit.
    inline constexpr size_t EIBI_MAX_DOWNLOAD_BYTES = 2U * 1024U * 1024U;

    struct EibiSeason {
        char schedule = 'a';
        int year = 0;

        std::string key() const;
    };

    EibiSeason eibiSeasonFor(const UtcDate& date);
    EibiSeason previousEibiSeason(const EibiSeason& season);

    class EibiSource : public ScheduleSource {
    public:
        const char* providerName() const override;
        std::vector<SourceTarget> targetsFor(const UtcDate& date) const override;
        bool parse(
            std::string_view payload,
            const SourceTarget& target,
            frequency_catalog::ProviderSnapshot& snapshot,
            ScheduleParseReport& report,
            std::string& error) const override;
    };

}
