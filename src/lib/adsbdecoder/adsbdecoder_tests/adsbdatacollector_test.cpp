#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "math.h"
#include <stdio.h>
#include "mockutils.h"
#include "mockconfig.h"
#include "ace/coreutils.hpp"

#define private public

#include "adsbdatacollector.hpp"

// This test needs a bit more validation
TEST_CASE("AdsbDataCollector", "[single-file]")
{
    AdsbDataCollector<100, 15'000'000> collector;
    REQUIRE(collector.size() == 0);
}

TEST_CASE("AdsbDataCollector start", "[single-file]")
{
    AdsbDataCollector<100, 15'000'000> collector;

    REQUIRE(collector.start(0x0000ff, 0) == true);
    REQUIRE(collector.size() == 1);
    REQUIRE(collector.start(0x000fff, 0) == true);
    REQUIRE(collector.size() == 2);

    REQUIRE(collector.start(0x0000ff, 0) == true);
    REQUIRE(collector.size() == 2);
    REQUIRE(collector.start(0x000fff, 0) == true);
    REQUIRE(collector.size() == 2);
}

TEST_CASE("AdsbDataCollector update and current", "[single-file]")
{
    AdsbDataCollector<100, 15'000'000> collector;

    collector.start(0x0000ff, 0);
    collector.updateAltitude(1234);

    collector.start(0x000fff, 0);
    collector.updateIcaoAddress("", 0);

    collector.start(0x0000ff, 0);
    auto current = collector.current();
    REQUIRE(current.gnsAltitude == 1234);
}

TEST_CASE("AdsbDataCollector Eviction", "[single-file]")
{
    AdsbDataCollector<100, 15'000'000> collector;

    // Insert entry will into future
    time_us_32Value = 1'000'000 * 10;
    REQUIRE(collector.start(0xffffff, time_us_32Value + 10'000 * 0) == true);
    collector.updateAltitude(1234);

    // Insert 100*100ms == 10 seconds of inserts
    int i;
    for (i = 0; i < 99; i++)
    {
        REQUIRE(collector.start(i, time_us_32Value + 10'000 * i) == true);
        collector.updateAltitude(i);
    }
    // Eviction will clean up every 25 entries, so left over with left, in this situation 56
    REQUIRE(collector.size() == 100);

    // Existing entry still exists
    REQUIRE(collector.start(0xffffff, time_us_32Value + 10'000 * (i + 5)) == true);
    REQUIRE(collector.current().gnsAltitude == 1234);

    // One additional entry later, eviction starts and left with one entry
    REQUIRE(collector.start(i+5, time_us_32Value + 1'000'000 * 30) == true);
    REQUIRE(collector.size() == 1);

    collector.dump();
}