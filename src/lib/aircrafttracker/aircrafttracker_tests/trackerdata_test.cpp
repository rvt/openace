
#include <catch2/catch_test_macros.hpp>

#define private public

#include "../ace/aircrafttracker.hpp"
#include "ace/messagerouter.hpp"
#include "ace/messages.hpp"
#include "pico/time.h"
#include "mockconfig.h"

constexpr uint32_t OUT_OF_ADAPTIVE_RANGE = 200000;

TEST_CASE("TrackerData ", "[single-file]")
{
    TrackerData<100, 2> trackedAircraft;
    REQUIRE(trackedAircraft.size() == 0);
}

TEST_CASE("TrackerData Insert within adaptiveRadius", "[single-file]")
{
    TrackerData<100, 2> trackedAircraft;

    OpenAce::AircraftPositionInfo aircraftPosition;
    aircraftPosition.timestamp = 560'000;
    aircraftPosition.distanceFromOwn = 10'000;
    REQUIRE(trackedAircraft.insert(aircraftPosition) == true);
    REQUIRE(trackedAircraft.size() == 1);

    SECTION("Uniqueness")
    {
        OpenAce::AircraftPositionInfo aircraftPosition;
        aircraftPosition.timestamp = 860'000;
        aircraftPosition.distanceFromOwn = 8000;
        REQUIRE(trackedAircraft.insert(aircraftPosition) == true);
        REQUIRE(trackedAircraft.size() == 1);
    }

    SECTION("Same, but now Out of adaptiveRadious and should not have been added")
    {
        OpenAce::AircraftPositionInfo aircraftPosition;
        aircraftPosition.timestamp = 860'000;
        aircraftPosition.address = 9999;
        aircraftPosition.distanceFromOwn = OUT_OF_ADAPTIVE_RANGE;
        REQUIRE(trackedAircraft.insert(aircraftPosition) == false);
        REQUIRE(trackedAircraft.size() == 1);
    }

    SECTION("next not called")
    {
        time_us_64Value = 120'000;
        auto delay = trackedAircraft.next([](const OpenAce::AircraftPositionInfo &position)
                                          { REQUIRE(false); });
        REQUIRE(delay == 380);
    }

    SECTION("next called with timeslice")
    {
        time_us_64Value = 900'000;
        bool called = false;
        auto delay = trackedAircraft.next([&called](const OpenAce::AircraftPositionInfo &position)
                                          { called = true; });
        REQUIRE(called);
        REQUIRE(delay == 100);

        // Would not call again with the same time as send time is updated
        called = false;
        delay = trackedAircraft.next([&called](const OpenAce::AircraftPositionInfo &position)
                                            { called = true; });
        REQUIRE(called == false);

        time_us_64Value = time_us_64Value + 250'000;
        delay = trackedAircraft.next([&called](const OpenAce::AircraftPositionInfo &position)
                                    { 
                                        REQUIRE(false);                                    
                                    });
        REQUIRE(delay == 350);
    }

    SECTION("next called runs stale")
    {
        time_us_64Value = aircraftPosition.timestamp + 10'000'000;
        trackedAircraft.maintenance();
        REQUIRE(trackedAircraft.size() == 0);
    }

    SECTION("Additional aircraft added and called first")
    {
        OpenAce::AircraftPositionInfo aircraftPosition;
        aircraftPosition.timestamp = 570'000;
        aircraftPosition.distanceFromOwn = 10000;
        aircraftPosition.address = 1;
        REQUIRE(trackedAircraft.insert(aircraftPosition) == true);
        REQUIRE(trackedAircraft.size() == 2);
        int callbacks = 0;
        time_us_64Value = 1'500'000;
        auto delay = trackedAircraft.next([&callbacks](const OpenAce::AircraftPositionInfo &position)
                                          {
                                        if (callbacks == 0 ) {
                                        REQUIRE (position.address == 0);
                                        }
                                        if (callbacks == 1 ) {
                                        REQUIRE (position.address == 1);
                                        }
                                        callbacks++; });
        REQUIRE(callbacks == 2);
    }
}

TEST_CASE("TrackerData Insert many and re-calculate adaptiveRadius ", "[single-file]")
{
    TrackerData<100, 2> trackedAircraft;

    int i = 0;
    for (; i < 100; i++)
    {
        OpenAce::AircraftPositionInfo aircraftPosition;
        aircraftPosition.distanceFromOwn = 10000 + 100 * i;
        aircraftPosition.address = i;
        REQUIRE(trackedAircraft.insert(aircraftPosition) == true);
    }
    REQUIRE(trackedAircraft.size() == 100);

    SECTION("One more should recalculate radius and must have free room")
    {
        REQUIRE(trackedAircraft.radius() == 75000);
        OpenAce::AircraftPositionInfo aircraftPosition;
        aircraftPosition.distanceFromOwn = 10000 + 100 * i;
        aircraftPosition.address = i;
        REQUIRE(trackedAircraft.insert(aircraftPosition) == true);
        REQUIRE(trackedAircraft.size() < 100); // Just be smaller than 100 really
        REQUIRE(trackedAircraft.size() > 85);  // But just not to much should have been removed
        REQUIRE(trackedAircraft.radius() == 18500);
    }

    SECTION("Must recalculate adaptive radius when planes added within")
    {
        auto radius = trackedAircraft.radius();
        for (int i = 125; i < 175; i++)
        {
            OpenAce::AircraftPositionInfo aircraftPosition;
            aircraftPosition.distanceFromOwn = i * 10;
            aircraftPosition.address = i;
            // Ensure that new planes are always added
            REQUIRE(trackedAircraft.insert(aircraftPosition) == true);
        }
        trackedAircraft.maintenance();
        // Check radious decreased
        REQUIRE(trackedAircraft.radius() < radius);

        SECTION("Keep calling to max radius size should increase to ADAPTIVE_RADIUS_MAX")
        {
            time_us_64Value = 12'000'000;
            for (i = 0; i < 100; i++)
            {
                trackedAircraft.maintenance();
            }
            REQUIRE(trackedAircraft.radius() == trackedAircraft.ADAPTIVE_RADIUS_MAX);
        }
    }
}

TEST_CASE("Next should handle correct delay 3 slices", "[single-file]")
{
    TrackerData<100, 3> trackedAircraft;
    etl::array time = {100'000, 450'000, 725'000};
    etl::array expectedDelay = {233, 216, 274};

    for (int i = 0; i < time.size(); i++)
    {
        time_us_64Value = time[i];
        auto delay = trackedAircraft.next([](const OpenAce::AircraftPositionInfo &position) {});
        REQUIRE(delay == expectedDelay[i]);
    }
}

TEST_CASE("Next should handle correct delay 4 slices", "[single-file]")
{
    TrackerData<100, 4> trackedAircraft;
    etl::array time = {1100'000, 1450'000, 28'725'000, 9'825'000};
    etl::array expectedDelay = {150, 50, 25, 175};

    for (int i = 0; i < time.size(); i++)
    {
        time_us_64Value = time[i];
        auto delay = trackedAircraft.next([](const OpenAce::AircraftPositionInfo &position) {});
        REQUIRE(delay == expectedDelay[i]);
    }
}

TEST_CASE("Should update data", "[single-file]")
{
    TrackerData<100, 4> trackedAircraft;

    OpenAce::AircraftPositionInfo aircraftPosition;
    aircraftPosition.timestamp = 570'000;
    aircraftPosition.distanceFromOwn = 10000;
    aircraftPosition.address = 1;

    // Insert aircraft
    REQUIRE(trackedAircraft.insert(aircraftPosition) == true);
    REQUIRE(trackedAircraft.size() == 1);

    // Insert same aircraft with other data tested by distanceFromOwn
    aircraftPosition.timestamp = 1'570'000;
    aircraftPosition.distanceFromOwn = 20000;
    REQUIRE(trackedAircraft.insert(aircraftPosition) == true);
    REQUIRE(trackedAircraft.size() == 1);

    // Validate if the queue was updated
    int callbacks = 0;
    time_us_64Value = 2'500'000;
    auto delay = trackedAircraft.next([&callbacks](const OpenAce::AircraftPositionInfo &position)
                                      {
                                        REQUIRE (position.distanceFromOwn == 20000);
                                        callbacks++; });
    REQUIRE(callbacks == 1);
}
