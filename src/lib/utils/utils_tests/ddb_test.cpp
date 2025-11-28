
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

TEST_CASE("Lookup Exact Icao", "[single-file]")
{
    DDB<10> ddb;
    auto lo = ddb.lookup(0x485024);
    REQUIRE(etl::string_view((*lo)->reg) == "PH-1523");
    REQUIRE(ddb.cacheSize() == 1);

    lo = ddb.lookup(0x48515D);
    REQUIRE(etl::string_view((*lo)->reg) == "PH-1552");
    REQUIRE(ddb.cacheSize() == 2);
}

TEST_CASE("Lookup Full Cache", "[single-file]")
{
    DDB<1> ddb;
    auto lo = ddb.lookup(0x485024);
    REQUIRE(etl::string_view((*lo)->reg) == "PH-1523");
    REQUIRE(ddb.cacheSize() == 1);

    lo = ddb.lookup(0x48515D);
    REQUIRE(etl::string_view((*lo)->reg) == "PH-1552");
    REQUIRE(ddb.cacheSize() == 1);
}

TEST_CASE("Lookup Not Found", "[single-file]")
{
    DDB<10> ddb;
    REQUIRE(ddb.cacheSize() == 0);

    auto lo = ddb.lookup(0x000800);
    REQUIRE_FALSE(lo.has_value()); // optional is present
    REQUIRE(ddb.cacheSize() == 1); // one negative-cache entry added

    lo = ddb.lookup(0x000800);
    REQUIRE_FALSE(lo.has_value()); // optional is present
    REQUIRE(ddb.cacheSize() == 1); // one negative-cache entry added

    lo = ddb.lookup(0x000801);
    REQUIRE_FALSE(lo.has_value()); // optional is present
    REQUIRE(ddb.cacheSize() == 2); // one negative-cache entry added
}

TEST_CASE("Show Duplicates", "[single-file]")
{

    for (size_t i = 1; i < DDB_COUNT; ++i)
    {
        if (DDB_DB[i].hex == DDB_DB[i - 1].hex)
        {
            std::cout << "Duplicate found: HEX=0x"
                      << std::hex << DDB_DB[i].hex
                      << " Callsign=" << DDB_DB[i].reg
                      << "\n";
        }
    }
}
