#pragma once

#include "ddb_db.hpp"
#include <etl/algorithm.h>
#include "ace/coreutils.hpp"

/**
 * Device Database to lookup CallSign from a HEX code.
 * The DB file must be generated with ddb.py.
 * The DB should for now only contain FLARM and OGN
 * entries where FLARM entries have priority during merging and generation
 * of the database
 */
class DDB
{
    const DDBEntry *lookupDb(uint32_t hex)
    {
        const uint8_t hi = (hex >> 16) & 0xFF;
        const uint16_t key = hex & 0xFFFF;
        const uint32_t start = DDB_BUCKET_START[hi];
        const uint32_t end = DDB_BUCKET_START[hi + 1];

        if (start == end)
        {
            return nullptr;
        }

        size_t left = start;
        size_t right = end;

        while (left < right)
        {
            size_t mid = (left + right) / 2;
            const uint16_t mid_key = DDB_KEYS[mid];

            if (mid_key < key)
            {
                left = mid + 1;
            }
            else
            {
                right = mid;
            }
        }

        if (left < end && DDB_KEYS[left] == key)
        {
            return &DDB_DB[left];
        }

        return nullptr;
    }

public:
    /**
     * Lookyo a hexcode in the DB. the result is nullptr for not found or
     * the entry in the DDB
     * @param hex
     * @return
     */
    const DDBEntry *lookup(uint32_t hex)
    {
        GATAS_MEASURE("lookup", 0);

        // The hexcode is swapped in the DDB_DB to ensure we have much more event buckets
        auto hexLookup = ((hex & 0x00FF0000) >> 16) |
                         ((hex & 0x000000FF) << 16) |
                         (hex & 0x0000FF00);

        return lookupDb(hexLookup);
    }
};
