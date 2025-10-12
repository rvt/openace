
#include <catch2/catch_test_macros.hpp>

#define private public

#include "../ace/aircrafttracker.hpp"
#include "ace/messagerouter.hpp"
#include "ace/messages.hpp"
#include "pico/time.h"
#include "mockconfig.h"

constexpr uint32_t OUT_OF_ADAPTIVE_RANGE = 200000;

class TestHandler
{
public:
    bool nextCalled = false;
    uint8_t callBacks = 0;
    void onNext(const GATAS::AircraftPositionInfo &) { nextCalled = true; callBacks += 1; }
};

TEST_CASE("TrackerData ", "[single-file]")
{
    TrackerData<100, 2> trackedAircraft;
    REQUIRE(trackedAircraft.size() == 0);
}

TEST_CASE("TrackerData Insert within adaptiveRadius", "[single-file]")
{
    time_us_Value = 0;
    TrackerData<100, 2> trackedAircraft;

    GATAS::AircraftPositionInfo aircraftPosition;
    aircraftPosition.timestamp = 560'000;
    aircraftPosition.distanceFromOwn = 10'000;
    REQUIRE(trackedAircraft.insert(aircraftPosition) == true);
    REQUIRE(trackedAircraft.size() == 1);

    SECTION("When adding, must stay unique")
    {
        GATAS::AircraftPositionInfo aircraftPosition;
        aircraftPosition.timestamp = 860'000;
        aircraftPosition.distanceFromOwn = 8000;
        REQUIRE(trackedAircraft.insert(aircraftPosition) == true);
        REQUIRE(trackedAircraft.size() == 1);
    }

    SECTION("Same, but now Out of adaptiveRadious and should not have been added")
    {
        GATAS::AircraftPositionInfo aircraftPosition;
        aircraftPosition.timestamp = 860'000;
        aircraftPosition.address = 9999;
        aircraftPosition.distanceFromOwn = OUT_OF_ADAPTIVE_RANGE;
        REQUIRE(trackedAircraft.insert(aircraftPosition) == false);
        REQUIRE(trackedAircraft.size() == 1);
    }

    SECTION("next Called, first one.. but not next one ")
    {
        TestHandler testHandler;
        time_us_Value = 120'000;
        auto delay = trackedAircraft.next(etl::delegate<void(const GATAS::AircraftPositionInfo &)>::create<TestHandler, &TestHandler::onNext>(testHandler));

        REQUIRE(testHandler.nextCalled);

        time_us_Value = 920'000;
        testHandler.nextCalled = false;
        delay = trackedAircraft.next(etl::delegate<void(const GATAS::AircraftPositionInfo &)>::create<TestHandler, &TestHandler::onNext>(testHandler));
        REQUIRE_FALSE(testHandler.nextCalled);

        REQUIRE(delay == 500);
    }

    SECTION("next called with timeslice")
    {
        time_us_Value = 900'000;
        TestHandler testHandler;
        auto delay = trackedAircraft.next(etl::delegate<void(const GATAS::AircraftPositionInfo &)>::create<TestHandler, &TestHandler::onNext>(testHandler));

        REQUIRE(testHandler.nextCalled);
        REQUIRE(delay == 500);

        // Would not call again with the same time as send time is updated
        testHandler.nextCalled = false;
        delay = trackedAircraft.next(etl::delegate<void(const GATAS::AircraftPositionInfo &)>::create<TestHandler, &TestHandler::onNext>(testHandler));

        time_us_Value = time_us_Value + 250'000;
        delay = trackedAircraft.next(etl::delegate<void(const GATAS::AircraftPositionInfo &)>::create<TestHandler, &TestHandler::onNext>(testHandler));

        REQUIRE(delay == 500);
    }

    SECTION("next called runs stale")
    {
        time_us_Value = aircraftPosition.timestamp + 11'000'000;
        trackedAircraft.maintenance();
        REQUIRE(trackedAircraft.size() == 0);
    }

    SECTION("Additional aircraft added ")
    {
        class TestHandler
        {
        public:
            int callBacks = 0;
            void onNext(const GATAS::AircraftPositionInfo &position)
            {
                if (callBacks == 0)
                {
                    REQUIRE(position.address == 0);
                }
                if (callBacks == 1)
                {
                    REQUIRE(position.address == 1);
                }
                callBacks += 1;
            }
        };

        GATAS::AircraftPositionInfo aircraftPosition;
        aircraftPosition.timestamp = 570'000;
        aircraftPosition.distanceFromOwn = 10000;
        aircraftPosition.address = 1;
        REQUIRE(trackedAircraft.insert(aircraftPosition) == true);
        REQUIRE(trackedAircraft.size() == 2);
        int callBacks = 0;
        time_us_Value = 1'500'000;
        TestHandler testHandler;
        auto delay = trackedAircraft.next(etl::delegate<void(const GATAS::AircraftPositionInfo &)>::create<TestHandler, &TestHandler::onNext>(testHandler));
        REQUIRE(testHandler.callBacks == 2);
    }
}

TEST_CASE("TrackerData Insert many and re-calculate adaptiveRadius ", "[single-file]")
{
    TrackerData<100, 2> trackedAircraft;

    int i = 0;
    for (; i < 100; i++)
    {
        GATAS::AircraftPositionInfo aircraftPosition;
        aircraftPosition.distanceFromOwn = 10000 + 100 * i;
        aircraftPosition.address = i;
        REQUIRE(trackedAircraft.insert(aircraftPosition) == true);
    }
    REQUIRE(trackedAircraft.size() == 100);

    SECTION("One more should recalculate radius and must have free room")
    {
        REQUIRE(trackedAircraft.radius() == 75000);
        GATAS::AircraftPositionInfo aircraftPosition;
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
            GATAS::AircraftPositionInfo aircraftPosition;
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
            time_us_Value = 12'000'000;
            for (i = 0; i < 100; i++)
            {
                trackedAircraft.maintenance();
            }
            REQUIRE(trackedAircraft.radius() == trackedAircraft.ADAPTIVE_RADIUS_MAX);
        }
    }
}

TEST_CASE("Next should handle correct delay 4 slices", "[single-file]")
{
    TrackerData<100, 4> trackedAircraft;
    TestHandler testHandler;
    auto delay = trackedAircraft.next(etl::delegate<void(const GATAS::AircraftPositionInfo &)>::create<TestHandler, &TestHandler::onNext>(testHandler));
    REQUIRE(delay == 250);
}

TEST_CASE("Should update data", "[single-file]")
{
    class TestHandler
    {
    public:
        int callBacks = 0;
        void onNext(const GATAS::AircraftPositionInfo &position)
        {
            REQUIRE(position.distanceFromOwn == 20000);
            callBacks += 1;
        }
    } testHandler;
    TrackerData<100, 4> trackedAircraft;

    time_us_Value = 1;
    GATAS::AircraftPositionInfo aircraftPosition;
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
    trackedAircraft.next(etl::delegate<void(const GATAS::AircraftPositionInfo &)>::create<TestHandler, &TestHandler::onNext>(testHandler));

    REQUIRE(testHandler.callBacks == 1);
}
