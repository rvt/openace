#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "math.h"
#include <stdio.h>
#include "mockutils.h"
#include "mockconfig.h"
#include "ace/coreutils.hpp"

#define private public

#include "addresscache.hpp"

// This test needs a bit more validation
TEST_CASE("Address Cache", "[single-file]")
{
    AddressCache<100, 30'000'000> cache;
    REQUIRE(cache.size() == 0);
}

TEST_CASE("Address Cache entry", "[single-file]")
{
    AddressCache<100, 30'000'000> cache;
    REQUIRE(cache.insert(0x123456, 10'000'000) == true);
    REQUIRE(cache.size() == 1);

    REQUIRE(cache.containsAndUpdate(0x123456, 35'000'000) == true);

    REQUIRE(cache.insert(0x123456, 40'000'000) == true);
    REQUIRE(cache.size() == 1);
}

TEST_CASE("Address Cache entry and evict", "[single-file]")
{
    auto offset = 100'000'000;
    auto spread = 90'000;
    AddressCache<100, 30'000'000> cache;
    for (int i = 0; i < 100; i++)
    {
        REQUIRE(cache.insert(i, 00'000'000) == true);
    }
    REQUIRE(cache.size() == 100);
    // THis add's about 10 seconds of data
    for (int i = 0; i < 100; i++)
    {
        REQUIRE(cache.containsAndUpdate(i, offset + spread * i) == true);
    }

    // So during cache evict, we end up with 50%
    REQUIRE(cache.insert(200, offset + spread * 101) == true);
    REQUIRE(cache.containsAndUpdate(200, offset + spread * 101) == true);
    REQUIRE(cache.size() > 1);
}
