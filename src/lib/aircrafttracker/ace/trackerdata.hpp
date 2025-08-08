#pragma once

#include "ace/coreutils.hpp"
#include "ace/constants.hpp"
#include "ace/measure.hpp"
#include "ace/models.hpp"

#include "etl/unordered_map.h"
#include "etl/set.h"
#include "etl/scaled_rounding.h"

/**
 * A tracker data queue for all aircraft received
 * The more TIMESLICES the more this code will spread the positional information in a second
 * because the code will ensure only a maximum of  trackedAircraft.size() / TIMESLICES will be send per each interval
 **/
template <size_t SIZE, uint8_t TIMESLICES>
class TrackerData
{
private:
    static constexpr int32_t SLICE_SIZE_MS = 1'000 / TIMESLICES;
    static constexpr int32_t MAX_POSITION_INTERPOLATIONS_USEC = 10'000'000;
    static constexpr int32_t TIME_SEND_HYSTERESIS = 100'000;
    static constexpr uint8_t ADAPTIVE_RADIUS_MIN_FREE = 4;
    static constexpr uint8_t ADAPTIVE_RADIUS_INCREASE_PERS = 75;
    static constexpr uint32_t HEARTBEAT_TIME = 1'000'000;
    static constexpr uint32_t ADAPTIVE_RADIUS_INCREASE = 1'000;
    static constexpr uint32_t ADAPTIVE_RADIUS_MAX = 100'000;

    struct TrackerEntry
    {
        uint32_t sendTime;
        GATAS::AircraftPositionInfo position;

        TrackerEntry(uint32_t time, const GATAS::AircraftPositionInfo &pos)
            : sendTime(time), position(pos) {}

        TrackerEntry() = default;
    };


    etl::unordered_map<GATAS::AircraftAddress, TrackerEntry, SIZE> trackedAircraft;
    uint32_t adaptiveRadius;

    bool calculateAdaptiveRadius()
    {
        if (trackedAircraft.size() >= (SIZE - ADAPTIVE_RADIUS_MIN_FREE))
        {
            etl::set<uint32_t, SIZE, etl::greater<uint32_t>> distances;
            for (const auto &pair : trackedAircraft)
            {
                distances.insert(pair.second.position.distanceFromOwn);
            }

            int8_t pos;
            if (distances.size() > SIZE / 2)
            {
                pos = SIZE - SIZE * 90 / 100;
            }
            else
            {
                pos = 1;
            }

            auto it = distances.begin();
            etl::advance(it, pos);
            adaptiveRadius = etl::round_floor_scaled<500>(*it);
            return true;
        }
        else
        {
            increaseAdaptiveRadius();
        }
        return false;
    }

    void increaseAdaptiveRadius()
    {
        if (trackedAircraft.size() < (SIZE * ADAPTIVE_RADIUS_INCREASE_PERS) / 100)
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
    TrackerData() : adaptiveRadius(75000) {}

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
        auto m = Measure("TrackerData::insert ", 400);
        if (position.distanceFromOwn > adaptiveRadius)
        {
            return false;
        }

        if (trackedAircraft.full())
        {
            if (!removeExpired())
            {
                calculateAdaptiveRadius();
                removeOutsideAdaptiveRadius();
            }
        }

        auto time = CoreUtils::timeUs32Raw();
        auto it = trackedAircraft.find(position.address);
        if (it != trackedAircraft.end())
        {
            it->second.sendTime = time;
            it->second.position = position;
        }
        else
        {
            trackedAircraft.insert({position.address, TrackerEntry(time, position)});
        }

        return true;
    }

    uint16_t next(const etl::delegate<void(const GATAS::AircraftPositionInfo &)> &msg)
    {
        auto currentTime = CoreUtils::timeUs32Raw();
        uint8_t count = 0;
        auto itemsPerSlice = static_cast<uint32_t>(trackedAircraft.size() / TIMESLICES);
        uint8_t maxPerRound = etl::max(static_cast<uint32_t>(5), itemsPerSlice);
        for (auto &pair : trackedAircraft)
        {
            auto &it = pair.second;
            if (CoreUtils::isUsReachedRaw(it.sendTime, currentTime))
            {
                msg(it.position);
                it.sendTime = currentTime + HEARTBEAT_TIME;
                if (count > maxPerRound)
                {
                    return SLICE_SIZE_MS;
                }
                count += 1;
            }
        }
        return SLICE_SIZE_MS;
    }

    void maintenance()
    {
        removeExpired();
        increaseAdaptiveRadius();
    }
};