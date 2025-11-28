#pragma once

#include "ddb_db.hpp"
#include <etl/algorithm.h>
#include <etl/unordered_map.h>
#include "ace/coreutils.hpp"

template <size_t CACHE_SIZE>
class DDB
{
    static constexpr uint32_t CACHE_VALID_TIME_US = 15'000'000;

    struct CacheEntry
    {
        const DDBEntry *ddbEntry;
        uint32_t timeAtInvalidation; // CoreUtils::timeUs32Raw() value
    };

    etl::unordered_map<uint32_t, CacheEntry, CACHE_SIZE> localCache;

    // Lookup entry in local cache first
    etl::optional<const DDBEntry *> lookupCache(uint32_t hex)
    {
        auto it = localCache.find(hex);
        if (it != localCache.end())
        {
            return it->second.ddbEntry; // wrap in optional
        }
        return {}; // empty optional
    }

    // Lookup entry in the main DB
    etl::optional<const DDBEntry *> lookupDb(uint32_t hex)
    {
        DDBEntry key = {hex, nullptr};

        auto it = etl::lower_bound(DDB_DB, DDB_DB + DDB_COUNT, key,
                                   [this](const DDBEntry &a, const DDBEntry &b)
                                   { return a.hex < b.hex; });

        if (it == DDB_DB + DDB_COUNT || it->hex != hex)
        {
            return {};
        }

        return it;
    }

public:
    /**
     * Indication of cache size
     */
    auto cacheSize() const
    {
        return localCache.size();
    }

    // Full lookup: cache first, then DB, then cache result
    etl::optional<const DDBEntry *> lookup(uint32_t hex)
    {
        auto optional = lookupCache(hex);
        if (optional)
        {
            if (*optional == nullptr)
            {
                return {};
            }
            else
            {
                return optional;
            }
        }

        puts("CAHCE FAIL");

        // Update cache (even nullptr to speed up next lookup)
        if (localCache.full())
        {
            for (auto it = localCache.begin(); it != localCache.end();)
            {
                if (CoreUtils::isUsReachedRaw(it->second.timeAtInvalidation))
                {
                    it = localCache.erase(it); // erase returns next iterator
                }
                else
                {
                    ++it;
                }
            }
        }

        auto ddBEntry = lookupDb(hex);
        if (!localCache.full())
        {
            if (ddBEntry)
            {
                localCache.insert({hex, {*ddBEntry, CoreUtils::timeUs32Raw() + CACHE_VALID_TIME_US}});
            }
            else
            {
                localCache.insert({hex, {nullptr, CoreUtils::timeUs32Raw() + CACHE_VALID_TIME_US}});
            }
            return ddBEntry;
        }

        return ddBEntry;
    }
};
