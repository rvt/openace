
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#define private public

#include "../ace/aircrafttracker.hpp"
#include "ace/messages.hpp"
#include "pico/time.h"
#include "mockconfig.h"

constexpr uint32_t OUT_OF_ADAPTIVE_RANGE = 200000;

static GATAS::OwnshipPositionInfo makeOwnship(float lat = 0.0f, float lon = 0.0f)
{
    GATAS::OwnshipPositionInfo ownship = {};
    ownship.lat = lat;
    ownship.lon = lon;
    return ownship;
}

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
    const auto ownship = makeOwnship();

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

    SECTION("A tracked aircraft is removed immediately when it leaves the adaptive radius")
    {
        REQUIRE(trackedAircraft.pathPredictor.contains(aircraftPosition.address));

        aircraftPosition.timestamp = 860'000;
        aircraftPosition.distanceFromOwn = OUT_OF_ADAPTIVE_RANGE;
        REQUIRE_FALSE(trackedAircraft.insert(aircraftPosition));
        REQUIRE(trackedAircraft.size() == 0);
        REQUIRE_FALSE(trackedAircraft.pathPredictor.contains(aircraftPosition.address));
    }

    SECTION("An older out-of-range position cannot remove a newer tracked aircraft")
    {
        aircraftPosition.timestamp = 559'999;
        aircraftPosition.distanceFromOwn = OUT_OF_ADAPTIVE_RANGE;
        REQUIRE_FALSE(trackedAircraft.insert(aircraftPosition));
        REQUIRE(trackedAircraft.size() == 1);
        REQUIRE(trackedAircraft.pathPredictor.contains(aircraftPosition.address));
    }

    SECTION("next Called, first one.. but not next one ")
    {
        TestHandler testHandler;
        time_us_Value = 120'000;
        trackedAircraft.sendScheduled(etl::delegate<void(const GATAS::AircraftPositionInfo &)>::create<TestHandler, &TestHandler::onNext>(testHandler), ownship);

        REQUIRE(testHandler.nextCalled);

        time_us_Value = 920'000;
        testHandler.nextCalled = false;
        trackedAircraft.sendScheduled(etl::delegate<void(const GATAS::AircraftPositionInfo &)>::create<TestHandler, &TestHandler::onNext>(testHandler), ownship);
        REQUIRE_FALSE(testHandler.nextCalled);

    }

    SECTION("next called with timeslice")
    {
        time_us_Value = 900'000;
        TestHandler testHandler;
        trackedAircraft.sendScheduled(etl::delegate<void(const GATAS::AircraftPositionInfo &)>::create<TestHandler, &TestHandler::onNext>(testHandler), ownship);

        REQUIRE(testHandler.nextCalled);

        // Would not call again with the same time as send time is updated
        testHandler.nextCalled = false;
        trackedAircraft.sendScheduled(etl::delegate<void(const GATAS::AircraftPositionInfo &)>::create<TestHandler, &TestHandler::onNext>(testHandler), ownship);

        time_us_Value = time_us_Value + 250'000;
        trackedAircraft.sendScheduled(etl::delegate<void(const GATAS::AircraftPositionInfo &)>::create<TestHandler, &TestHandler::onNext>(testHandler), ownship);
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
        trackedAircraft.sendScheduled(etl::delegate<void(const GATAS::AircraftPositionInfo &)>::create<TestHandler, &TestHandler::onNext>(testHandler), ownship);
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
    const auto ownship = makeOwnship();

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
    trackedAircraft.sendScheduled(delegate, ownship);
    REQUIRE(testHandler.callBacks == 2);

    // Second slot at t=600ms: remaining 2 still have sendTime=0, fires 2
    time_us_Value = 600'000;
    trackedAircraft.sendScheduled(delegate, ownship);
    REQUIRE(testHandler.callBacks == 4);

    // Calling again at the same time: all sendTimes are in the future, nothing fires
    trackedAircraft.sendScheduled(delegate, ownship);
    REQUIRE(testHandler.callBacks == 4);

    //Time advances
    time_us_Value = time_us_Value + 500'000;
    trackedAircraft.sendScheduled(delegate, ownship);
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
    const auto ownship = makeOwnship();

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
    trackedAircraft.sendScheduled(etl::delegate<void(const GATAS::AircraftPositionInfo &)>::create<TestHandler, &TestHandler::onNext>(testHandler), ownship);

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

TEST_CASE("Path predictor keeps the closest tracked aircraft when full", "[single-file]")
{
    TrackerData<8, 2, 6> trackedAircraft;
    trackedAircraft.pathPrediction(true);

    for (uint32_t i = 0; i < 6; ++i)
    {
        GATAS::AircraftPositionInfo aircraftPosition;
        aircraftPosition.address = 10 + i;
        aircraftPosition.timestamp = 1'000'000 + i;
        aircraftPosition.distanceFromOwn = 1000 * (i + 1);
        REQUIRE(trackedAircraft.insert(aircraftPosition) == true);
        REQUIRE(trackedAircraft.pathPredictor.contains(aircraftPosition.address));
    }

    REQUIRE(trackedAircraft.pathPredictor.size() == 6);
    REQUIRE(trackedAircraft.pathPredictor.contains(10));
    REQUIRE(trackedAircraft.pathPredictor.contains(15));

    GATAS::AircraftPositionInfo closerNew;
    closerNew.address = 20;
    closerNew.timestamp = 2'000'000;
    closerNew.distanceFromOwn = 1500;
    REQUIRE(trackedAircraft.insert(closerNew) == true);

    REQUIRE_FALSE(trackedAircraft.pathPredictor.contains(15));
    REQUIRE(trackedAircraft.pathPredictor.contains(20));
    REQUIRE(trackedAircraft.pathPredictor.contains(10));

    GATAS::AircraftPositionInfo fartherNew;
    fartherNew.address = 21;
    fartherNew.timestamp = 3'000'000;
    fartherNew.distanceFromOwn = 9000;
    REQUIRE(trackedAircraft.insert(fartherNew) == true);

    REQUIRE_FALSE(trackedAircraft.pathPredictor.contains(21));
    REQUIRE(trackedAircraft.pathPredictor.contains(20));
    REQUIRE(trackedAircraft.pathPredictor.contains(10));
}

TEST_CASE("Path predictor tracks all aircraft when predictor size matches tracker size", "[single-file]")
{
    TrackerData<10, 2, 10> trackedAircraft;
    trackedAircraft.pathPrediction(true);

    for (uint32_t i = 0; i < 8; ++i)
    {
        GATAS::AircraftPositionInfo aircraftPosition;
        aircraftPosition.address = 30 + i;
        aircraftPosition.timestamp = 1'000'000 + i;
        aircraftPosition.distanceFromOwn = 1000 * (i + 1);
        REQUIRE(trackedAircraft.insert(aircraftPosition) == true);
        REQUIRE(trackedAircraft.pathPredictor.contains(aircraftPosition.address));
    }

    REQUIRE(trackedAircraft.pathPredictor.size() == 8);

    GATAS::AircraftPositionInfo fartherNew;
    fartherNew.address = 50;
    fartherNew.timestamp = 2'000'000;
    fartherNew.distanceFromOwn = 9'000;
    REQUIRE(trackedAircraft.insert(fartherNew) == true);

    REQUIRE(trackedAircraft.pathPredictor.contains(30));
    REQUIRE(trackedAircraft.pathPredictor.contains(37));
    REQUIRE(trackedAircraft.pathPredictor.contains(50));
}

TEST_CASE("Path predictor derives turn behavior from recent history", "[single-file]")
{
    TrackerData<10, 2, 10> trackedAircraft;
    trackedAircraft.pathPrediction(true);

    GATAS::AircraftPositionInfo aircraftPosition;
    aircraftPosition.address = 60;
    aircraftPosition.dataSource = GATAS::DataSource::OGN;
    aircraftPosition.lat = 52.0f;
    aircraftPosition.lon = 4.0f;
    aircraftPosition.ellipseHeight = 1000;
    aircraftPosition.groundSpeed = 50.0f;
    aircraftPosition.distanceFromOwn = 1000;
    aircraftPosition.timestamp = 0;
    aircraftPosition.track = 350;
    aircraftPosition.hTurnRate = NAN;
    REQUIRE(trackedAircraft.insert(aircraftPosition) == true);

    aircraftPosition.timestamp = 1'000'000;
    aircraftPosition.lat = 52.00045f;
    aircraftPosition.track = 0;
    REQUIRE(trackedAircraft.insert(aircraftPosition) == true);

    aircraftPosition.timestamp = 2'000'000;
    aircraftPosition.lat = 52.00089f;
    aircraftPosition.lon = 4.00008f;
    aircraftPosition.track = 10;
    REQUIRE(trackedAircraft.insert(aircraftPosition) == true);

    GATAS::AircraftPositionInfo predicted = trackedAircraft.pathPredictor.extrapolatedPos(3'000'000, aircraftPosition);
    REQUIRE(predicted.track == 20);
}

TEST_CASE("adslUplinkTrigger returns predicted output positions when path prediction is enabled", "[single-file]")
{
    time_us_Value = 0;
    TrackerData<10, 2, 10> trackedAircraft;
    trackedAircraft.pathPrediction(true);

    auto ownship = makeOwnship(52.0f, 4.0f);
    GATAS::AircraftPositionInfo aircraftPosition;
    aircraftPosition.address = 70;
    aircraftPosition.timestamp = 0;
    aircraftPosition.lat = 52.0f;
    aircraftPosition.lon = 4.0f;
    aircraftPosition.ellipseHeight = 1000;
    aircraftPosition.groundSpeed = 50.0f;
    aircraftPosition.track = 90;
    aircraftPosition.distanceFromOwn = 0;
    REQUIRE(trackedAircraft.insert(aircraftPosition) == true);

    time_us_Value = 2'000'000;
    auto closest = trackedAircraft.adslUplinkTrigger(ownship);
    REQUIRE(closest.size() == 1);
    REQUIRE(closest[0].timestamp == 2'000'000);
    REQUIRE(static_cast<float>(closest[0].distanceFromOwn) == Catch::Approx(100.0f).margin(2.0f));

    auto rel = CoreUtils::northEastDistance(52.0f, 4.0f, closest[0].lat, closest[0].lon);
    REQUIRE(rel.east == Catch::Approx(100.0f).margin(1.0f));
}

TEST_CASE("Path predictor keeps history while prediction output is disabled", "[single-file]")
{
    time_us_Value = 0;
    TrackerData<10, 2, 10> trackedAircraft;
    auto ownship = makeOwnship(52.0f, 4.0f);

    GATAS::AircraftPositionInfo aircraftPosition;
    aircraftPosition.address = 71;
    aircraftPosition.timestamp = 0;
    aircraftPosition.lat = 52.0f;
    aircraftPosition.lon = 4.0f;
    aircraftPosition.groundSpeed = 50.0f;
    aircraftPosition.track = 90;
    aircraftPosition.distanceFromOwn = 0;
    REQUIRE(trackedAircraft.insert(aircraftPosition) == true);
    REQUIRE(trackedAircraft.pathPredictor.contains(aircraftPosition.address));

    time_us_Value = 2'000'000;
    auto closest = trackedAircraft.adslUplinkTrigger(ownship);
    REQUIRE(closest.size() == 1);
    REQUIRE(closest[0].timestamp == 0);

    trackedAircraft.pathPrediction(true);
    closest = trackedAircraft.adslUplinkTrigger(ownship);
    REQUIRE(closest.size() == 1);
    REQUIRE(closest[0].timestamp == 2'000'000);
    REQUIRE(static_cast<float>(closest[0].distanceFromOwn) == Catch::Approx(100.0f).margin(2.0f));
}

TEST_CASE("Predicted output does not mutate stored original distance", "[single-file]")
{
    time_us_Value = 0;
    TrackerData<10, 2, 10> trackedAircraft;
    trackedAircraft.pathPrediction(true);

    auto ownship = makeOwnship(52.0f, 4.0f);
    GATAS::AircraftPositionInfo aircraftPosition;
    aircraftPosition.address = 72;
    aircraftPosition.timestamp = 0;
    aircraftPosition.lat = 52.0f;
    aircraftPosition.lon = 4.0f;
    aircraftPosition.groundSpeed = 50.0f;
    aircraftPosition.track = 90;
    aircraftPosition.distanceFromOwn = 0;
    REQUIRE(trackedAircraft.insert(aircraftPosition) == true);

    time_us_Value = 2'000'000;
    auto closest = trackedAircraft.adslUplinkTrigger(ownship);
    REQUIRE(closest.size() == 1);
    REQUIRE(static_cast<float>(closest[0].distanceFromOwn) == Catch::Approx(100.0f).margin(2.0f));

    auto it = trackedAircraft.trackedAircraft.find(72);
    REQUIRE(it != trackedAircraft.trackedAircraft.end());
    REQUIRE(it->second.position.timestamp == 0);
    REQUIRE(it->second.position.distanceFromOwn == 0);
}

TEST_CASE("TrackerData rejects out-of-order position updates", "[single-file]")
{
    time_us_Value = 2'000'000;
    TrackerData<10, 2, 10> trackedAircraft;
    trackedAircraft.pathPrediction(true);

    GATAS::AircraftPositionInfo latest;
    latest.address = 73;
    latest.timestamp = 2'000'000;
    latest.lat = 52.0f;
    latest.lon = 4.0f;
    latest.groundSpeed = 50.0f;
    latest.track = 90;
    latest.distanceFromOwn = 1'000;
    REQUIRE(trackedAircraft.insert(latest));

    time_us_Value = 2'500'000;
    GATAS::AircraftPositionInfo older = latest;
    older.timestamp = 1'000'000;
    older.lat = 51.0f;
    older.lon = 3.0f;
    older.distanceFromOwn = 2'000;
    REQUIRE_FALSE(trackedAircraft.insert(older));

    auto stored = trackedAircraft.trackedAircraft.find(latest.address);
    REQUIRE(stored != trackedAircraft.trackedAircraft.end());
    REQUIRE(stored->second.position.timestamp == latest.timestamp);
    REQUIRE(stored->second.position.lat == latest.lat);
    REQUIRE(stored->second.position.lon == latest.lon);
    REQUIRE(stored->second.position.distanceFromOwn == latest.distanceFromOwn);
    REQUIRE(stored->second.sendTime == 2'000'000);
}

TEST_CASE("Radio priority: RADIO 4000000us old, ADSB incoming - should NOT update", "[single-file]")
{
    TrackerData<100, 4> trackedAircraft;
    time_us_Value = 0;
    const auto ownship = makeOwnship();

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
    trackedAircraft.sendScheduled(etl::delegate<void(const GATAS::AircraftPositionInfo &)>::create<VerifyNotUpdatedHandler, &VerifyNotUpdatedHandler::onNext>(handler), ownship);
}

TEST_CASE("Radio priority: RADIO 5000000us old, ADSB incoming - should UPDATE", "[single-file]")
{
    TrackerData<100, 4> trackedAircraft;
    time_us_Value = 0;
    const auto ownship = makeOwnship();

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
    trackedAircraft.sendScheduled(etl::delegate<void(const GATAS::AircraftPositionInfo &)>::create<VerifyUpdatedHandler, &VerifyUpdatedHandler::onNext>(handler), ownship);
}

TEST_CASE("Data source prefix preserves fixed callsign length", "[single-file]")
{
    TrackerData<100, 4> trackedAircraft;
    trackedAircraft.prefixEnabled(true);
    const auto ownship = makeOwnship();

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

    trackedAircraft.sendScheduled(etl::delegate<void(const GATAS::AircraftPositionInfo &)>::create<VerifyPrefixLengthHandler, &VerifyPrefixLengthHandler::onNext>(handler), ownship);
}

TEST_CASE("Data source prefix does create callsigns for empty values", "[single-file]")
{
    TrackerData<100, 4> trackedAircraft;
    trackedAircraft.prefixEnabled(true);
    const auto ownship = makeOwnship();

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

    trackedAircraft.sendScheduled(etl::delegate<void(const GATAS::AircraftPositionInfo &)>::create<VerifyEmptyHandler, &VerifyEmptyHandler::onNext>(handler), ownship);
}

TEST_CASE("Squawk display replaces callsign when squawk is known", "[single-file]")
{
    time_us_Value = 0;
    TrackerData<100, 4> trackedAircraft;
    trackedAircraft.prefixEnabled(true);
    trackedAircraft.showSquawk(true);
    const auto ownship = makeOwnship();

    GATAS::AircraftPositionInfo aircraftPosition;
    aircraftPosition.distanceFromOwn = 5000;
    aircraftPosition.dataSource = GATAS::DataSource::ADSB;
    aircraftPosition.callSign = "PH-ABC";
    aircraftPosition.squawk = 42;

    REQUIRE(trackedAircraft.insert(aircraftPosition) == true);

    class VerifySquawkHandler
    {
    public:
        uint8_t callCount = 0;

        void onNext(const GATAS::AircraftPositionInfo &position)
        {
            REQUIRE(position.callSign == "AB0042");
            callCount += 1;
        }
    } handler;

    auto callback = etl::delegate<void(const GATAS::AircraftPositionInfo &)>::create<VerifySquawkHandler, &VerifySquawkHandler::onNext>(handler);
    trackedAircraft.sendScheduled(callback, ownship);
    REQUIRE(handler.callCount == 1);

    aircraftPosition.timestamp = 1;
    aircraftPosition.callSign = "PH-DEF";
    time_us_Value = 1'100'000;

    REQUIRE(trackedAircraft.insert(aircraftPosition) == true);
    trackedAircraft.sendScheduled(callback, ownship);
    REQUIRE(handler.callCount == 2);
}
