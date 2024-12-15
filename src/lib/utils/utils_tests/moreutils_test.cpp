
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#define private public

#include <stdio.h>

#include "moreutils.hpp"

TEST_CASE("Every", "[single-file]")
{

    SECTION("Every at a time", "[single-file]")
    {
        Every<uint32_t, 500'000, 1'000'000> every;
        REQUIRE(every.isItTime(100) == true);
        REQUIRE(every.isItTime(999'999) == false);
        REQUIRE(every.isItTime(1'000'000) == false);
        REQUIRE(every.isItTime(1'500'000) == true);
        REQUIRE(every.isItTime(1'600'000) == false);
        REQUIRE(every.isItTime(2'400'000) == false);
        REQUIRE(every.isItTime(2'500'000) == true);
        REQUIRE(every.isItTime(2'500'000) == false);
    }

    SECTION("zero", "[single-file]")
    {
        Every<uint32_t, 0, 1'000'000> every;
        REQUIRE(every.isItTime(100) == true);
        REQUIRE(every.isItTime(999'999) == false);
        REQUIRE(every.isItTime(1'000'001) == true);
        REQUIRE(every.isItTime(1'500'000) == false);
        REQUIRE(every.isItTime(1'600'000) == false);
        REQUIRE(every.isItTime(2'400'000) == true);
        REQUIRE(every.isItTime(2'500'000) == false);
        REQUIRE(every.isItTime(2'600'000) == false);
        REQUIRE(every.isItTime(3'000'000) == true);
    }

    SECTION("when wraps", "[single-file]")
    {
        Every<uint32_t, 100'000, 1'000'000> every{UINT32_MAX-4'099'999};
        REQUIRE(every.isItTime(UINT32_MAX-4'100'000) == false); // Due to wrap around
        REQUIRE(every.isItTime(UINT32_MAX-3'100'000) == true); // Due to wrap around
        REQUIRE(every.isItTime(UINT32_MAX-500'00) == true);
        REQUIRE(every.isItTime(132704) == true); // Due to wrap around we will see a one time jitter. THis should be fixed one day
        REQUIRE(every.isItTime(232704) == false); 
        REQUIRE(every.isItTime(1'100'000) == true);
    }
}
