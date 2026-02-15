#pragma once

#include "etl/bitset.h"
#include "etl/vector.h"
#include "etl/span.h"

namespace GATAS
{

    template <size_t MaxSources>
    class DataSourceTimeStatsTable
    {
    public:
        struct DataSourceTimeStats
        {
            etl::bitset<100> timeTenthMs{};
            uint32_t frequency = 0;
        };

        using container_type = etl::vector<DataSourceTimeStats, MaxSources>;
        using const_span_type = etl::span<const DataSourceTimeStats>;

        /**
         * @param frequency On what frequency this was received
         * @param msInSecond value of 0..100 of the ms within teh second aligned to PPS
         */
        void addReceiveStat(uint32_t frequency, uint16_t msInSecond)
        {
            GATAS_ASSERT(msInSecond < 1000, "Must be smaller than 1000");

            auto tenthMsIndex = 99 - msInSecond / 10;

            for (auto &&stat : stats_)
            {
                if (stat.frequency == frequency)
                {
                    stat.timeTenthMs.set(tenthMsIndex);
                    return;
                }
            }

            if (!stats_.full())
            {
                DataSourceTimeStats stat{};
                stat.frequency = frequency;
                stat.timeTenthMs.set(tenthMsIndex);
                stats_.emplace_back(stat);
            }
        }

        const_span_type span() const
        {
            return const_span_type(stats_.data(), stats_.size());
        }

        size_t size() const
        {
            return stats_.size();
        }

        bool empty() const
        {
            return stats_.empty();
        }

        void clear()
        {
            stats_.clear();
        }

    private:
        container_type stats_{};
    };

}
