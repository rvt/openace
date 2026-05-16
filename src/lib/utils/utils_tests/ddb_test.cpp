
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "math.h"

#define private public

#include <stdio.h>
#include <iostream>

#define __in_flash()
// #include "ddb_db.hpp"
#include "ddb.hpp"
#include "testhelpers.h"

TEST_CASE("Lookup hex above and below mid", "[single-file]")
{
    DDB ddb;
    auto lo = ddb.lookup(0xDF1634);

    REQUIRE(lo != nullptr);
    REQUIRE(etl::string_view(lo->reg()) == "D-9866");
    //    REQUIRE(ddb.cacheSize() == 1);

    lo = ddb.lookup(0x404E39);
    REQUIRE(lo != nullptr);
    REQUIRE(etl::string_view(lo->reg()) == "G-CDNA");
    //    REQUIRE(ddb.cacheSize() == 2);
}

TEST_CASE("Unique OGN F DB Entry", "[single-file]")
{
    DDB ddb;
    auto lo = ddb.lookup(0x000203);

    REQUIRE(lo != nullptr);
    REQUIRE(etl::string_view(lo->reg()) == "SRZ2000");
}

TEST_CASE("OGN Entry dropped", "[single-file]")
{
    DDB ddb;
    auto lo = ddb.lookup(0x0DA550);

    REQUIRE(lo == nullptr);
}

TEST_CASE("Prefer FLARM DB Entry", "[single-file]")
{
    DDB ddb;
    auto lo = ddb.lookup(0xDD4EBE);

    REQUIRE(lo != nullptr);
    REQUIRE(etl::string_view(lo->reg()) == "G-CKOL");
}

TEST_CASE("ICAO Should not be in DB", "[single-file]")
{
    DDB ddb;
    // 0x48515D is an entry that was ICAO only, that should not appear in GATAS
    auto lo = ddb.lookup(0x48515D);
    REQUIRE(lo == nullptr);
}

TEST_CASE("Removes invalid callsigns", "[single-file]")
{
    DDB ddb;
    REQUIRE(ddb.lookup(0x200508) == nullptr); // Starts with a number
    REQUIRE(ddb.lookup(0x254342) == nullptr); // TEST
    REQUIRE(ddb.lookup(0x88E240) == nullptr); // TST    
    REQUIRE(ddb.lookup(0x88C1D0) == nullptr); // Two Chars only    
    REQUIRE(ddb.lookup(0x111913) == nullptr); // GLIDER    
}

TEST_CASE("Lookup First and Last", "[single-file]")
{
    DDB ddb;
    auto lo = ddb.lookup(0x000000);

    REQUIRE(lo != nullptr);
    REQUIRE(etl::string_view(lo->reg()) == "OY-XEL");

    lo = ddb.lookup(0xffffff); // Note: Found in FLARM DDB, so expect this one, not the one from OGN
    REQUIRE(lo != nullptr);
    REQUIRE(etl::string_view(lo->reg()) == "D-ETIG");
}

// TEST_CASE("Lookup Full Cache", "[single-file]")
// {
//     DDB ddb;
//     auto lo = ddb.lookup(0x485024);
//     REQUIRE(lo != nullptr);
//     REQUIRE(etl::string_view(lo->reg()) == "PH-1523");
//     REQUIRE(ddb.cacheSize() == 1);

//     lo = ddb.lookup(0x48515D);
//     REQUIRE(lo != nullptr);
//     REQUIRE(etl::string_view(lo->reg()) == "PH-1552");
//     REQUIRE(ddb.cacheSize() == 1);
// }

TEST_CASE("Lookup Not Found", "[single-file]")
{
    DDB ddb;
    //    REQUIRE(ddb.cacheSize() == 0);

    auto lo = ddb.lookup(0x000800);
    REQUIRE(lo == nullptr);
    //    REQUIRE(ddb.cacheSize() == 1);

    lo = ddb.lookup(0x000800);
    REQUIRE(lo == nullptr);
    //    REQUIRE(ddb.cacheSize() == 1);

    lo = ddb.lookup(0x000801);
    REQUIRE(lo == nullptr);
    //    REQUIRE(ddb.cacheSize() == 2);
}

TEST_CASE("No Duplicates", "[single-file]")
{
    size_t bucket = 0;
    uint32_t previousHex = (uint32_t(bucket) << 16) | DDB_KEYS[0];

    for (size_t i = 1; i < DDB_COUNT; ++i)
    {
        while ((bucket + 1) < 257 && DDB_BUCKET_START[bucket + 1] <= i)
        {
            bucket += 1;
        }

        const uint32_t currentHex = (uint32_t(bucket) << 16) | DDB_KEYS[i];
        if (currentHex == previousHex)
        {
            std::cout << "Duplicate found: HEX=0x"
                      << std::hex << currentHex
                      << " Callsign=" << DDB_DB[i].reg()
                      << "\n";
            REQUIRE_FALSE(true);
        }
        previousHex = currentHex;
    }
}
