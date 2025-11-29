
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "math.h"

#define private public

#include <stdio.h>
#include <iostream>

#define __in_flash()
// #include "ddb_db.hpp"
#include "ddb.hpp"
#include "mockutils.h"

TEST_CASE("Lookup Icao above and below mid", "[single-file]")
{
    DDB ddb;
    auto lo = ddb.lookup(0xDF1634);

    REQUIRE(lo != nullptr);
    REQUIRE(etl::string_view(lo->reg()) == "D-9866");
    //    REQUIRE(ddb.cacheSize() == 1);

    lo = ddb.lookup(0x48515D);
    REQUIRE(lo != nullptr);
    REQUIRE(etl::string_view(lo->reg()) == "PH-1552");
    //    REQUIRE(ddb.cacheSize() == 2);
}

TEST_CASE("Lookup First and Last", "[single-file]")
{
    DDB ddb;
    auto lo = ddb.lookup(0x000000);

    REQUIRE(lo != nullptr);
    REQUIRE(etl::string_view(lo->reg()) == "HA-4403");
    //    REQUIRE(ddb.cacheSize() == 1);

    lo = ddb.lookup(0xffffff);
    REQUIRE(lo != nullptr);
    REQUIRE(etl::string_view(lo->reg()) == "D-KEBW");
    //    REQUIRE(ddb.cacheSize() == 2);
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
