#pragma once

#include "ace/coreutils.hpp"
#include "ace/constants.hpp"
#include "ace/models.hpp"
#include "ace/ddb.hpp"

#include "etl/array.h"
#include "etl/unordered_map.h"
#include "etl/scaled_rounding.h"
#include "etl/algorithm.h"

/**
 * A tracker data queue for all aircraft received
 * The more TIMESLICES the more this code will spread the positional information in a second
 * because the code will ensure only a maximum of  trackedAircraft.size() / TIMESLICES will be send per each interval
 **/
template <size_t SIZE, uint8_t TIMESLICES>
class TrackerData
{
private:
    // Try to ensure that XX is minimally free to allow for burst of new aircraft
    static constexpr uint8_t ADAPTIVE_RADIUS_MIN_FREE = 4;
    // When less than X persentage the buffers is full, start ioncreaing the adaptive radius
    static constexpr uint8_t ADAPTIVE_RADIUS_PERCENTAGE_INCREASE = 75;
    // When buffer needs to cleanup because nearly full, keep XX percebtage of all aircraft
    static constexpr uint8_t ADAPTIVE_RADIUS_PERCENTAGE_KEEP = 90;
    static constexpr uint32_t HEARTBEAT_TIME = 1'000'000;
    static constexpr uint32_t ADAPTIVE_RADIUS_INCREASE = 1'000;
    static constexpr uint32_t ADAPTIVE_RADIUS_MAX = 100'000;
    static constexpr int32_t SLICE_SIZE_MS = 1'000 / TIMESLICES;
    static constexpr int32_t MAX_POSITION_INTERPOLATIONS_USEC = 10'000'000;
    static constexpr int32_t TIME_SEND_HYSTERESIS = 100'000;
    // Prefer RADIO positions over others for this number of us
    static constexpr int32_t RADIO_PRIORITY_TIMEOUT_US = 4000000;

    struct TrackerEntry
    {
        uint32_t sendTime;
        GATAS::AircraftPositionInfo position;

        TrackerEntry(uint32_t time, const GATAS::AircraftPositionInfo &pos)
            : sendTime(time), position(pos) {}

        TrackerEntry() = default;
    };

    struct ByDistance
    {
        bool operator()(const GATAS::AircraftPositionInfo *a, const GATAS::AircraftPositionInfo *b) const
        {
            if (a->distanceFromOwn != b->distanceFromOwn)
            {
                return a->distanceFromOwn < b->distanceFromOwn;
            }
            return a < b; // pointer as unique tiebreaker
        }
    };

    etl::unordered_map<GATAS::AircraftAddress, TrackerEntry, SIZE> trackedAircraft;
    DDB ddb;
    uint32_t adaptiveRadius;
    bool ddbLookupsEnabled_;
    bool prefixEnabled_;

    /**
     * Recalculate the adaptive tracking radius when the tracker is nearly full.
     *
     * The algorithm collects the current aircraft distances from ownship,
     * sorts them from farthest to nearest, removes duplicate distance values,
     * and then selects a cutoff near the far end of the list. That cutoff is
     * rounded down to the nearest 500 meters and stored as the new
     * adaptiveRadius. The effect is that, under pressure, the farthest aircraft
     * are trimmed first while the majority of nearer traffic is retained.
     *
     * @return true if a new adaptive radius was calculated, otherwise false.
     */
    bool calculateAdaptiveRadius()
    {
        auto size = trackedAircraft.size();
        if (size >= (SIZE - ADAPTIVE_RADIUS_MIN_FREE))
        {
            etl::array<uint32_t, SIZE> distances = {};
            size_t count = 0;
            for (const auto &pair : trackedAircraft)
            {
                distances[count] = pair.second.position.distanceFromOwn;
                count += 1;
            }

            etl::sort(distances.begin(), distances.begin() + count, etl::greater<uint32_t>());

            size_t uniqueCount = 0;
            for (size_t i = 0; i < count; ++i)
            {
                if (uniqueCount == 0 || distances[i] != distances[uniqueCount - 1])
                {
                    distances[uniqueCount] = distances[i];
                    uniqueCount += 1;
                }
            }

            // Find the 90% position, that means we remove 10% of the aircraft based on radious
            size_t pos = 1;
            if (uniqueCount > SIZE / 2)
            {
                pos = SIZE - SIZE * ADAPTIVE_RADIUS_PERCENTAGE_KEEP / 100;
                if (pos >= uniqueCount)
                {
                    pos = uniqueCount - 1;
                }
            }

            // Calculate new adaptive radious
            adaptiveRadius = etl::round_floor_scaled<500>(distances[pos]);
            return true;
        }
        else if (size < SIZE - ADAPTIVE_RADIUS_MIN_FREE)
        {
            increaseAdaptiveRadius();
        }
        return false;
    }

    void increaseAdaptiveRadius()
    {
        if (trackedAircraft.size() < (SIZE * ADAPTIVE_RADIUS_PERCENTAGE_INCREASE) / 100)
        {
            adaptiveRadius = etl::min(adaptiveRadius + ADAPTIVE_RADIUS_INCREASE, ADAPTIVE_RADIUS_MAX);
        }
    }

    bool removeExpired()
    {
        bool cleaned = false;
        uint32_t us = CoreUtils::timeUs32Raw();

        for (auto it = trackedAircraft.begin(); it != trackedAircraft.end();)
        {
            if (CoreUtils::isUsReachedRaw(it->second.position.timestamp + MAX_POSITION_INTERPOLATIONS_USEC, us))
            {
                it = trackedAircraft.erase(it);
                cleaned = true;
            }
            else
            {
                ++it;
            }
        }

        return cleaned;
    }

    void removeOutsideAdaptiveRadius()
    {
        for (auto it = trackedAircraft.begin(); it != trackedAircraft.end();)
        {
            if (it->second.position.distanceFromOwn >= adaptiveRadius)
            {
                it = trackedAircraft.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

public:
    TrackerData() : adaptiveRadius(75000), ddbLookupsEnabled_(false), prefixEnabled_(false) {}

    template <typename Callback>
    void forEachPosition(const Callback &callback) const
    {
        for (const auto &pair : trackedAircraft)
        {
            callback(pair.second.position);
        }
    }

    void ddbEnabled(bool enabled)
    {
        ddbLookupsEnabled_ = enabled;
    }

    void prefixEnabled(bool enabled)
    {
        prefixEnabled_ = enabled;
    }

    bool full() const
    {
        return trackedAircraft.full();
    }

    uint16_t size() const
    {
        return static_cast<uint16_t>(trackedAircraft.size());
    }

    uint32_t radius() const
    {
        return adaptiveRadius;
    }

    void dump() const
    {
        int c = 0;
        for (const auto &pair : trackedAircraft)
        {
            const auto &it = pair.second;
            printf("%3d icao:%6lX sendTime:%8ld time:%8ld  dist:%ld lat:%.6f lon:%.6f\n",
                   c, it.position.address, it.sendTime, CoreUtils::timeUs32Raw(),
                   it.position.distanceFromOwn, it.position.lat, it.position.lon);
            c += 1;
        }
    }

    bool insert(GATAS::AircraftPositionInfo &position)
    {
        GATAS_MEASURE("insert", 400);
        if (position.distanceFromOwn > adaptiveRadius)
        {
            return false;
        }

        auto time = CoreUtils::timeUs32Raw();
        auto it = trackedAircraft.find(position.address);
        if (it != trackedAircraft.end())
        {
            it->second.sendTime = time;

            assignCallsignFromDDB(position);
            assignDataSourcePrefix(position);

            // Prefer MLAT over ADSB for RADIO_PRIORITY_TIMEOUT_US to avoid jumps.
            // We assume ADSB/MLAT data is less acurate due to delays in the chain
            // TODO: We should revise this once all timings are validated (they are within a second, bit in finer detail)
            const bool trackedIsRadio = it->second.position.dataSource != GATAS::DataSource::ADSB;
            const bool incomingIsMlat = position.dataSource == GATAS::DataSource::ADSB;
            const bool radioStillFresh = !CoreUtils::isUsReachedRaw(it->second.position.timestamp + RADIO_PRIORITY_TIMEOUT_US, time);
            if (trackedIsRadio && incomingIsMlat && radioStillFresh)
            {
                return false;
            }

            it->second.position = position;
            return true;
        }

        if (trackedAircraft.full())
        {
            if (!removeExpired())
            {
                calculateAdaptiveRadius();
                removeOutsideAdaptiveRadius();
            }
        }

        GATAS_VERIFY(!trackedAircraft.full(), "TrackerData: Should never be full");
        if (trackedAircraft.full())
        {
            return false;
        }

        assignCallsignFromDDB(position);
        assignDataSourcePrefix(position);

        trackedAircraft.insert({position.address, TrackerEntry(time, position)});
        return true;
    }

    /**
     * Assign a callsign from DDB if the callsign field is empty
     * @param position
     */
    void assignCallsignFromDDB(GATAS::AircraftPositionInfo &position)
    {
        if (ddbLookupsEnabled_ && position.callSign.empty())
        {
            auto ddbEntry = ddb.lookup(position.address);
            if (ddbEntry)
            {
                position.callSign = ddbEntry->reg();
            }
        }
    }

    void assignDataSourcePrefix(GATAS::AircraftPositionInfo &position)
    {
        if (prefixEnabled_)
        {
            GATAS::CallSign prefixedCallSign(GATAS::toShortString(position.dataSource));
            position.callSign.insert(0, prefixedCallSign);
        }
    }

    void sendScheduled(const etl::delegate<void(const GATAS::AircraftPositionInfo &)> &callback)
    {
        auto currentTime = CoreUtils::timeUs32Raw();
        size_t count = 0;
        size_t maxPerRound = (trackedAircraft.size() + TIMESLICES - 1) / TIMESLICES;
        for (auto &pair : trackedAircraft)
        {
            auto &it = pair.second;
            if (CoreUtils::isUsReachedRaw(it.sendTime, currentTime))
            {
                callback(it.position);
                count += 1;
                it.sendTime = currentTime + HEARTBEAT_TIME;
                if (count >= maxPerRound)
                {
                    return;
                }
            }
        }
    }

    void maintenance()
    {
        removeExpired();
        increaseAdaptiveRadius();
    }

    /**
     * Return up to the 10 closest tracked aircraft.
     *
     * When fewer than 10 aircraft are currently tracked, all of them are
     * returned immediately. Otherwise, the algorithm builds a fixed-size heap
     * of 10 candidate aircraft. The heap root is the farthest aircraft in the
     * current candidate set. Each additional aircraft only replaces that root
     * if it is closer, which leaves the 10 closest aircraft in the heap at the
     * end of the scan. The return order is not significant.
     *
     * @return A list containing up to the 10 nearest tracked aircraft.
     */
    GATAS::AdslObandUplinkAircraft forClosest() const
    {
        GATAS_MEASURE("forClosest", 1200);
        GATAS::AdslObandUplinkAircraft result;

        // SHortcut with < 10 aircraft
        if (trackedAircraft.size() <= result.max_size())
        {
            for (const auto &pair : trackedAircraft)
            {
                result.push_back(pair.second.position);
            }
            return result;
        }

        etl::array<const GATAS::AircraftPositionInfo *, 10> closest = {};
        size_t count = 0;
        auto it = trackedAircraft.begin();
        for (; it != trackedAircraft.end() && count < closest.size(); ++it)
        {
            closest[count] = &it->second.position;
            count += 1;
        }

        etl::make_heap(closest.begin(), closest.begin() + count, ByDistance());

        for (; it != trackedAircraft.end(); ++it)
        {
            const auto *candidate = &it->second.position;
            if (ByDistance()(candidate, closest.front()))
            {
                etl::pop_heap(closest.begin(), closest.begin() + count, ByDistance());
                closest[count - 1] = candidate;
                etl::push_heap(closest.begin(), closest.begin() + count, ByDistance());
            }
        }

        for (size_t i = 0; i < count; ++i)
        {
            result.push_back(*closest[i]);
        }

        return result;
    }
};
