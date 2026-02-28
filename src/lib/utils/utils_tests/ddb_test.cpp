
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

TEST_CASE("Unique OGN DB Entry", "[single-file]")
{
    DDB ddb;
    auto lo = ddb.lookup(0x066F73);

    REQUIRE(lo != nullptr);
    REQUIRE(etl::string_view(lo->reg()) == "OK-0566");
}

TEST_CASE("Unique FLARM DB Entry", "[single-file]")
{
    DDB ddb;
    auto lo = ddb.lookup(0xDD4EBE);

    REQUIRE(lo != nullptr);
    REQUIRE(etl::string_view(lo->reg()) == "G-CKOL");
}

TEST_CASE("ICAO Should not be in DB", "[single-file]")
{
    DDB ddb;
    // 0x48515D is an entry that was ICAO only, that should not appear in GA/TAS
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
    REQUIRE(etl::string_view(lo->reg()) == "HA-4403");

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
    for (size_t i = 1; i < DDB_COUNT; ++i)
    {
        if (DDB_DB[i].hex() == DDB_DB[i - 1].hex())
        {
            std::cout << "Duplicate found: HEX=0x"
                      << std::hex << DDB_DB[i].hex()
                      << " Callsign=" << DDB_DB[i].reg()
                      << "\n";
            REQUIRE_FALSE(true);
        }
    }
}
