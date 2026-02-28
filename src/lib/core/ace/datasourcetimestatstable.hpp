#pragma once

#include "etl/bitset.h"
#include "etl/vector.h"
#include "etl/span.h"
#include "etl/array.h"

namespace GATAS
{

    /**
     * @brief Helper class to store reception times of messages from different data sources.
     *        This can be used to understand if timings are correct.
     */
    class DataSourceTimeStatsTableBase
    {
    public:
        struct DataSourceTimeStats
        {
            etl::bitset<100> timeTenthMs{};
            uint32_t frequency = 0;
        };

        using const_span_type   = etl::span<const DataSourceTimeStats>;
        using mutable_span_type = etl::span<DataSourceTimeStats>;

        void addReceiveStat(uint32_t frequency, uint16_t msInSecond)
        {
            GATAS_ASSERT(msInSecond < 1000, "Must be smaller than 1000");

            auto tenthMsIndex = 99 - msInSecond / 10;

            for (auto &&stat : used_span())
            {
                if (stat.frequency == frequency)
                {
                    stat.timeTenthMs.set(tenthMsIndex);
                    return;
                }
            }

            if (size_ < storage_.size())
            {
                auto &stat = storage_[size_++];
                stat = DataSourceTimeStats{};
                stat.frequency = frequency;
                stat.timeTenthMs.set(tenthMsIndex);
            }
        }

        const_span_type span() const { return const_span_type(storage_.data(), size_); }
        size_t size() const { return size_; }
        bool empty() const { return size_ == 0; }
        void clear() { size_ = 0; }

    protected:
        DataSourceTimeStatsTableBase() = default;

        void init(mutable_span_type storage)
        {
            GATAS_ASSERT(storage.data() != nullptr, "Storage must be valid");
            storage_ = storage;
            size_ = 0;
        }

        mutable_span_type used_span()
        {
            return mutable_span_type(storage_.data(), size_);
        }

    private:
        mutable_span_type storage_{};
        size_t size_ = 0;
    };

    // Templated wrapper — only owns storage, zero logic here
    template <size_t MaxSources>
    class DataSourceTimeStatsTable : public DataSourceTimeStatsTableBase
    {
    public:
        DataSourceTimeStatsTable()
        {
            init(mutable_span_type(buffer_.data(), MaxSources));
        }

    private:
        etl::array<DataSourceTimeStats, MaxSources> buffer_{};
    };

} // namespace GATAS