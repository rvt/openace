
#include <catch2/catch_test_macros.hpp>

#define private public

#include "../ace/aircrafttracker.hpp"
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
        trackedAircraft.sendScheduled(etl::delegate<void(const GATAS::AircraftPositionInfo &)>::create<TestHandler, &TestHandler::onNext>(testHandler));

        REQUIRE(testHandler.nextCalled);

        time_us_Value = 920'000;
        testHandler.nextCalled = false;
        trackedAircraft.sendScheduled(etl::delegate<void(const GATAS::AircraftPositionInfo &)>::create<TestHandler, &TestHandler::onNext>(testHandler));
        REQUIRE_FALSE(testHandler.nextCalled);

    }

    SECTION("next called with timeslice")
    {
        time_us_Value = 900'000;
        TestHandler testHandler;
        trackedAircraft.sendScheduled(etl::delegate<void(const GATAS::AircraftPositionInfo &)>::create<TestHandler, &TestHandler::onNext>(testHandler));

        REQUIRE(testHandler.nextCalled);

        // Would not call again with the same time as send time is updated
        testHandler.nextCalled = false;
        trackedAircraft.sendScheduled(etl::delegate<void(const GATAS::AircraftPositionInfo &)>::create<TestHandler, &TestHandler::onNext>(testHandler));

        time_us_Value = time_us_Value + 250'000;
        trackedAircraft.sendScheduled(etl::delegate<void(const GATAS::AircraftPositionInfo &)>::create<TestHandler, &TestHandler::onNext>(testHandler));
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
        trackedAircraft.sendScheduled(etl::delegate<void(const GATAS::AircraftPositionInfo &)>::create<TestHandler, &TestHandler::onNext>(testHandler));
        REQUIRE(testHandler.callBacks == 1);
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

TEST_CASE("sendScheduled distributes 4 aircraft across 2 timeslices", "[single-file]")
{
    // With TIMESLICES=2 and 4 aircraft: maxPerRound = ceil(4/2) = 2.
    // First sendScheduled fires 2, sets their sendTime = currentTime + HEARTBEAT_TIME.
    // Second sendScheduled 500ms later fires the remaining 2 (still at sendTime=0).
    time_us_Value = 0;
    TrackerData<100, 2> trackedAircraft;

    for (uint32_t i = 0; i < 4; ++i)
    {
        GATAS::AircraftPositionInfo ac;
        ac.address = i;
        ac.timestamp = 0;
        ac.distanceFromOwn = 5000;
        trackedAircraft.insert(ac);
    }
    REQUIRE(trackedAircraft.size() == 4);

    TestHandler testHandler;
    auto delegate = etl::delegate<void(const GATAS::AircraftPositionInfo &)>::create<TestHandler, &TestHandler::onNext>(testHandler);

    // First slot at t=0: maxPerRound = ceil(4/2) = 2, fires 2
    trackedAircraft.sendScheduled(delegate);
    REQUIRE(testHandler.callBacks == 2);

    // Second slot at t=600ms: remaining 2 still have sendTime=0, fires 2
    time_us_Value = 600'000;
    trackedAircraft.sendScheduled(delegate);
    REQUIRE(testHandler.callBacks == 4);

    // Calling again at the same time: all sendTimes are in the future, nothing fires
    trackedAircraft.sendScheduled(delegate);
    REQUIRE(testHandler.callBacks == 4);

    //Time advances
    time_us_Value = time_us_Value + 500'000;
    trackedAircraft.sendScheduled(delegate);
    REQUIRE(testHandler.callBacks == 6);
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
    trackedAircraft.sendScheduled(etl::delegate<void(const GATAS::AircraftPositionInfo &)>::create<TestHandler, &TestHandler::onNext>(testHandler));

    REQUIRE(testHandler.callBacks == 1);
}

TEST_CASE("Updating an existing aircraft must not trigger full-buffer cleanup", "[single-file]")
{
    TrackerData<4, 2> trackedAircraft;

    for (uint32_t i = 0; i < 4; ++i)
    {
        GATAS::AircraftPositionInfo aircraftPosition;
        aircraftPosition.address = i;
        aircraftPosition.distanceFromOwn = 1000 + 1000 * i;
        REQUIRE(trackedAircraft.insert(aircraftPosition) == true);
    }

    REQUIRE(trackedAircraft.size() == 4);
    REQUIRE(trackedAircraft.radius() == 75000);

    GATAS::AircraftPositionInfo update;
    update.address = 0;
    update.distanceFromOwn = 1500;
    REQUIRE(trackedAircraft.insert(update) == true);

    REQUIRE(trackedAircraft.size() == 4);
    REQUIRE(trackedAircraft.radius() == 75000);
}

TEST_CASE("Radio priority: RADIO 4000000us old, ADSB incoming - should NOT update", "[single-file]")
{
    TrackerData<100, 4> trackedAircraft;
    time_us_Value = 0;

    // Insert radio data at t=0
    GATAS::AircraftPositionInfo radioPosition;
    radioPosition.address = 42;
    radioPosition.timestamp = 0;
    radioPosition.distanceFromOwn = 5000;
    radioPosition.dataSource = GATAS::DataSource::OGN;
    REQUIRE(trackedAircraft.insert(radioPosition) == true);

    // At t=2000000us, incoming ADSB data arrives
    // Radio is 2000000us old, well within priority timeout (4000000us)
    time_us_Value = 2000000;
    GATAS::AircraftPositionInfo adsbPosition;
    adsbPosition.address = 42;
    adsbPosition.timestamp = 2000000;
    adsbPosition.distanceFromOwn = 5100;
    adsbPosition.dataSource = GATAS::DataSource::ADSB;

    REQUIRE(trackedAircraft.insert(adsbPosition) == false);
    REQUIRE(trackedAircraft.size() == 1);

    // Verify radio data was NOT replaced
    class VerifyNotUpdatedHandler
    {
    public:
        void onNext(const GATAS::AircraftPositionInfo &position)
        {
            REQUIRE(position.distanceFromOwn == 5000);  // Should be original radio data
            REQUIRE(position.dataSource == GATAS::DataSource::OGN);
        }
    } handler;
    time_us_Value = 2000100;
    trackedAircraft.sendScheduled(etl::delegate<void(const GATAS::AircraftPositionInfo &)>::create<VerifyNotUpdatedHandler, &VerifyNotUpdatedHandler::onNext>(handler));
}

TEST_CASE("Radio priority: RADIO 5000000us old, ADSB incoming - should UPDATE", "[single-file]")
{
    TrackerData<100, 4> trackedAircraft;
    time_us_Value = 0;

    // Insert radio data at t=0
    GATAS::AircraftPositionInfo radioPosition;
    radioPosition.address = 42;
    radioPosition.timestamp = 0;
    radioPosition.distanceFromOwn = 5000;
    radioPosition.dataSource = GATAS::DataSource::OGN;
    REQUIRE(trackedAircraft.insert(radioPosition) == true);

    // At t=5000000us, incoming ADSB data arrives
    // Radio is 5000000us old, should exceed priority timeout
    time_us_Value = 5000000;
    GATAS::AircraftPositionInfo adsbPosition;
    adsbPosition.address = 42;
    adsbPosition.timestamp = 5000000;
    adsbPosition.distanceFromOwn = 5100;
    adsbPosition.dataSource = GATAS::DataSource::ADSB;

    REQUIRE(trackedAircraft.insert(adsbPosition) == true);
    REQUIRE(trackedAircraft.size() == 1);

    // Verify ADSB data WAS inserted
    class VerifyUpdatedHandler
    {
    public:
        void onNext(const GATAS::AircraftPositionInfo &position)
        {
            REQUIRE(position.distanceFromOwn == 5100);  // Should be updated ADSB data
            REQUIRE(position.dataSource == GATAS::DataSource::ADSB);
        }
    } handler;
    time_us_Value = 5000100;
    trackedAircraft.sendScheduled(etl::delegate<void(const GATAS::AircraftPositionInfo &)>::create<VerifyUpdatedHandler, &VerifyUpdatedHandler::onNext>(handler));
}

TEST_CASE("Data source prefix preserves fixed callsign length", "[single-file]")
{
    TrackerData<100, 4> trackedAircraft;
    trackedAircraft.prefixEnabled(true);

    GATAS::AircraftPositionInfo aircraftPosition;
    aircraftPosition.distanceFromOwn = 5000;
    aircraftPosition.dataSource = GATAS::DataSource::FLARM;
    aircraftPosition.callSign = "PH-ABCDEFGHIJKL";

    REQUIRE(trackedAircraft.insert(aircraftPosition) == true);

    class VerifyPrefixLengthHandler
    {
    public:
        void onNext(const GATAS::AircraftPositionInfo &position)
        {
            REQUIRE(position.callSign == "FLPH-ABCDEFG");
        }
    } handler;

    trackedAircraft.sendScheduled(etl::delegate<void(const GATAS::AircraftPositionInfo &)>::create<VerifyPrefixLengthHandler, &VerifyPrefixLengthHandler::onNext>(handler));
}

TEST_CASE("Data source prefix does create callsigns for empty values", "[single-file]")
{
    TrackerData<100, 4> trackedAircraft;
    trackedAircraft.prefixEnabled(true);

    GATAS::AircraftPositionInfo aircraftPosition;
    aircraftPosition.distanceFromOwn = 5000;
    aircraftPosition.dataSource = GATAS::DataSource::ADSB;

    REQUIRE(trackedAircraft.insert(aircraftPosition) == true);

    class VerifyEmptyHandler
    {
    public:
        void onNext(const GATAS::AircraftPositionInfo &position)
        {
            REQUIRE(position.callSign == "AB");
        }
    } handler;

    trackedAircraft.sendScheduled(etl::delegate<void(const GATAS::AircraftPositionInfo &)>::create<VerifyEmptyHandler, &VerifyEmptyHandler::onNext>(handler));
}
