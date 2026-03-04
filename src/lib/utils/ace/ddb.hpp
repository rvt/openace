#pragma once

#include "ddb_db.hpp"
#include <etl/algorithm.h>
#include <etl/unordered_map.h>
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
        uint8_t hi = (hex >> 16) & 0xFF;
        const uint32_t start = DDB_INDEX[hi].start;
        const uint32_t count = DDB_INDEX[hi].count;

        if (count == 0)
        {
            return nullptr;
        }

        size_t left = start;
        size_t right = start + count;

        while (left < right)
        {
            size_t mid = (left + right) / 2;
            uint32_t mid_hex = DDB_DB[mid].hex();

            if (mid_hex < hex)
            {
                left = mid + 1;
            }
            else
            {
                right = mid;
            }
        }

        if (left < start + count && DDB_DB[left].hex() == hex)
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
