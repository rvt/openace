#pragma once

#include "ace/coreutils.hpp"
#include "ace/constants.hpp"
#include "ace/measure.hpp"
#include "ace/models.hpp"

#include "etl/forward_list.h"
#include "etl/scaled_rounding.h"

/**
 * A tracker data queue for all aircraft received
 * Some protocols like GDL90 requires 1 second heartbeat for all aircraft and this tracker facilitates this.
 * In near future it will also advance positions based on historical locations (heading + speed) to ensure
 * a moving map works.acosfAny aircraft that is not updated for some time will be removed from the queue
 * A tracker must ensure all aircraft are send at least every second. An new aircraft as soon as possible (within 500ms seems reasonable)
 *
 * Considerations when making modifications:
 * Maximum of 32 inserts/updates per second for GLD90
 * Aircraft can be add at any moment in time
 * Aircraft can be removed if the start to fall outside a radius
 * Aircraft can get stale after xx seconds of no update
 * Ideally it should send aircraft in batches per timeslice this is not implemented in this
 *
 * For this implementation it was decided to keep it simple.
 * Just a simple set that it's iterated over each time slice to test if any aircraft needs to be send.
 * If an aircraft was send, add a time of 1second to the next time to send. THis means 32 comparisons per time slice which seems reasonable.
 *
 */
template <size_t SIZE, uint8_t TIMESLICES>
class TrackerData
{
private:
    static constexpr int32_t SLICE_SIZE_MS = 1'000 / TIMESLICES;            // How much time for each slic, use to calculate delay
    static constexpr int32_t MAX_POSITION_INTERPOLATIONS_USEC = 10'000'000; // Maximum number of times we interpolate position of a aircraft when it was not received, after that we removed it from the tracker
    static constexpr int32_t TIME_SEND_HYSTERESIS = 100'000;                // How much time before/after the current ms new entries will be send
    static constexpr uint8_t ADAPTIVE_RADIUS_MIN_FREE = 4;                  // Do adaptive calculations when queuesize is > (SIZE - ADAPTIVE_RADIUS_MIN_FREE)
    static constexpr uint8_t ADAPTIVE_RADIUS_INCREASE_PERS = 75;            // From what percentage the adaptive RADIUS increases with ADAPTIVE_RADIUS_INCREASE
    static constexpr uint32_t HEARTBEAT_TIME = 1'000'000;                   // At which heartbeat to send out the aircraft
    static constexpr uint32_t ADAPTIVE_RADIUS_INCREASE = 1'000;             // If there is room (less than ADAPTIVE_RADIUS_INCREASE_PERS% queue full) increase the radious by this value
    static constexpr uint32_t ADAPTIVE_RADIUS_MAX = 100'000;                // Maximum radius the system considers tracking aicraft in.

    struct TrackerEntry
    {
        uint32_t sendTime;
        OpenAce::AircraftPositionInfo position;

        // Add this constructor:
        TrackerEntry(uint32_t time, const OpenAce::AircraftPositionInfo &pos)
            : sendTime(time), position(pos) {}

        TrackerEntry(const TrackerEntry &) = delete;
        TrackerEntry &operator=(const TrackerEntry &) = delete;
        TrackerEntry(TrackerEntry &&) = delete;
        TrackerEntry &operator=(TrackerEntry &&) = delete;

        bool operator==(const TrackerEntry &other) const
        {
            return position.address == other.position.address;
        }

        bool operator<(const TrackerEntry &other) const
        {
            return position.address < other.position.address;
        }
    };

    etl::forward_list<TrackerEntry, SIZE> trackedAircraft;
    uint32_t adaptiveRadius;

    /**
     * Based on current dataset calculate adaptive radius
     * 1) When the queue is nearly full \sa ADAPTIVE_RADIUS_MIN_FREE , a new range is calculated at the same of ADAPTIVE_RADIUS_MIN_FREE
     * 2) When the queue is getting empty, increase adaptiveRadius
     *
     * Returns true if adaptive radius was decreased
     */
    bool calculateAdaptiveRadius()
    {
        if (trackedAircraft.size() >= (SIZE - ADAPTIVE_RADIUS_MIN_FREE))
        {
            etl::set<uint32_t, SIZE, etl::greater<uint32_t>> distances;
            for (const auto &it : trackedAircraft)
            {
                distances.insert(it.position.distanceFromOwn);
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

    /**
     * Increase adaptive radius when the queue is less than ADAPTIVE_RADIUS_INCREASE_PERS full
     */
    void increaseAdaptiveRadius()
    {
        if (trackedAircraft.size() < (SIZE * ADAPTIVE_RADIUS_INCREASE_PERS) / 100)
        {
            adaptiveRadius = etl::min(adaptiveRadius + ADAPTIVE_RADIUS_INCREASE, ADAPTIVE_RADIUS_MAX);
        }
    }

    /**
     * Remove expired aircraft when not updated for MAX_POSITION_INTERPOLATIONS_USEC
     * returns true when aircraft where removed
     */
    bool removeExpired()
    {
        bool cleaned = false;
        auto prev = trackedAircraft.before_begin();
        auto it = trackedAircraft.begin();

        while (it != trackedAircraft.end())
        {
            if (CoreUtils::isUsReached((it->position.timestamp + MAX_POSITION_INTERPOLATIONS_USEC)))
            {
                it = trackedAircraft.erase_after(prev);
                cleaned = true;
            }
            else
            {
                prev = it;
                ++it;
            }
        }

        return cleaned;
    }

    /**
     * Remove all aircraft that are outside the adaptive radius
     */
    void removeOutsideAdaptiveRadius()
    {
        auto prev = trackedAircraft.before_begin();
        auto it = trackedAircraft.begin();

        while (it != trackedAircraft.end())
        {
            if (it->position.distanceFromOwn >= adaptiveRadius)
            {
                it = trackedAircraft.erase_after(prev);
            }
            else
            {
                prev = it;
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
        for (const auto &it : trackedAircraft)
        {
            printf("%3d icao:%6lX sendTime:%8ld time:%8ld  dist:%ld lat:%.6f lon:%.6f\n",
                   c, it.position.address, it.sendTime, CoreUtils::timeUs32(),
                   it.position.distanceFromOwn, it.position.lat, it.position.lon);
            c++;
        }
    }

    /**
     * Insert a position in front of the queue
     */
    bool insert(OpenAce::AircraftPositionInfo &position)
    {
        auto m = Measure("TrackerData::insert", 400);
        // Never insert outside of adaptive radius
        if (position.distanceFromOwn > adaptiveRadius)
        {
            // printf("Adaptive> t:%08ld %06lX\n", CoreUtils::timeUs32() / 1'000'000, position.address);
            return false;
        }

        if (trackedAircraft.full())
        {
            if (!removeExpired())
            {
                // calculateAdaptiveRadius will  ensure something is removed, no need for additional checks
                calculateAdaptiveRadius(/*Todo add distance for this new aircraft so removal can be made smarter*/);
                removeOutsideAdaptiveRadius();
            }
        }

        // Check if the aircraft already exists (update instead of insert)
        auto prev = trackedAircraft.before_begin();
        auto it = trackedAircraft.begin();
        while (it != trackedAircraft.end())
        {
            if (it->position.address == position.address)
            {
                // Update existing entry
                it->sendTime = CoreUtils::timeUs32Raw();
                it->position = position;
                return true;
            }
            prev = it;
            ++it;
        }

        // Insert new entry at the front (O(1))
        trackedAircraft.emplace_front(CoreUtils::timeUs32Raw(), position);
        return true;
    }

    /**
     * Take the items from the front of the queue and place it to the back
     * If the que has zero or 1 items, don't do anything.
     * If the trackEstimates is >= 10 then erase this entry
     */
    uint16_t next(const etl::delegate<void(const OpenAce::AircraftPositionInfo &)> &msg)
    {
        auto currentTime = CoreUtils::timeUs32Raw();
        for (auto &it : trackedAircraft)
        {
            if (CoreUtils::isUsReached(it.sendTime, currentTime))
            {
                msg(it.position);
                it.sendTime = currentTime + HEARTBEAT_TIME;
            }
        }
        return SLICE_SIZE_MS;
    }

    /**
     * Do regular maitenance on the queue
     * - Removes stail aircraft
     * - Increases adaptive radius if possible
     */
    void maintenance()
    {
        removeExpired();
        increaseAdaptiveRadius();
    }
};