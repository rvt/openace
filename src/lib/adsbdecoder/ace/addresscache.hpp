#pragma once

#include "iaddresscache.hpp"

#include "ace/coreutils.hpp"
#include "ace/constants.hpp"
#include "ace/models.hpp"

#include "etl/flat_set.h"

template <size_t SIZE, int32_t EVICT_TIME_US>
class AddressCache : public IAddressCache
{
private:
    // 80% means that when the cache is full, eviction will happen till the cache is 80% full
    static constexpr int32_t CLEAR_UP_SIZE = (SIZE * 90) / 100;
    static constexpr int32_t EVICT_STEP_US = 5'000'000;

    struct CacheEntry
    {
        GATAS::AircraftAddress icao;
        uint32_t lastSeen;

        // Default constructor
        CacheEntry() = default;

        CacheEntry(GATAS::AircraftAddress icao_, uint32_t lastSeen_)
            : icao(icao_), lastSeen(lastSeen_) {}

        CacheEntry(const CacheEntry &other)
            : icao(other.icao), lastSeen(other.lastSeen) {}

        CacheEntry &operator=(const CacheEntry &other)
        {
            icao = other.icao;
            lastSeen = other.lastSeen;
            return *this;
        }

        constexpr bool operator<(const CacheEntry &other) const
        {
            return icao < other.icao;
        }

        constexpr bool operator()(const CacheEntry &lhs, const CacheEntry &rhs) const
        {
            return lhs.icao < rhs.icao;
        }
    };

    etl::flat_set<CacheEntry, SIZE, CacheEntry> cache;

public:
    void clear()
    {
        cache.clear();
    }

    size_t size() const
    {
        return cache.size();
    }

    /**
     * Validate if the cache contains icao, if so then update the timestamp
     */
    bool ifContainsThenUpdate(uint32_t icao, uint32_t usTime)
    {
        auto it = cache.find(CacheEntry{icao, usTime});
        if (it != cache.end())
        {
            it->lastSeen = usTime;
            return true;
        }

        return false;
    }

    bool insert(uint32_t address, uint32_t usTime)
    {
        if (cache.full())
        {
            return false;
        }
        cache.insert(CacheEntry{address, usTime});

        return true;
    }

    /**
     * This method remove all older entries trying to find entries on a best efford.
     * It assumes that there is some form of distribution of airplanes coming in.
     * 100 planes per second in a cache of 100 is never going to work nice
     * THis could be done better to only evict olders entries.
     */
    void evictOldEntries(uint32_t usTime)
    {
        constexpr int32_t EVICT_TIME_MINIMUM = 2'000'000;

        // Always ensure there is room for new cache entries be reducing evictTime untill there is room again
        auto evictTime = EVICT_TIME_US;
        while ((cache.size() > CLEAR_UP_SIZE) && evictTime > EVICT_TIME_MINIMUM)
        {
            for (auto it = cache.begin(); it != cache.end();)
            {
                if (-CoreUtils::usToReferenceRaw(it->lastSeen, usTime) > evictTime)
                {
                    it = cache.erase(it);
                }
                else
                {
                    ++it;
                }
            }

            evictTime -= EVICT_STEP_US;
        }
    }
};
