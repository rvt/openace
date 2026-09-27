#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <limits>

#include "ace/ownshipstate.hpp"
#include "ace/coreutils.hpp"

TEST_CASE("Ownship state stores independent location and pressure snapshots", "[ownshipstate]")
{
    CoreUtils::init();
    GATAS::OwnshipState state;
    state.init(CoreUtils::sharedSpinLock());
    const auto &reader = state;

    REQUIRE(reader.location.load().timestamp == 0);
    REQUIRE(reader.barometricPressure.load().pressurehPa == GATAS::INVALID_PRESSSURE_HPA);
    REQUIRE(reader.pressureAltQnh.load().pressureAlt == GATAS::INVALID_BARO_ALTITUDE);
    REQUIRE(reader.pressureAltQnh.load().qnh == GATAS::INVALID_QNH);

    GATAS::OwnshipPositionInfo location{};
    location.timestamp = 1234;
    location.lat = 52.0f;
    location.lon = 5.0f;
    location.ellipseHeight = 450;
    state.location.store(location);
    const auto snapshot = reader.location.load();

    state.barometricPressure.store(GATAS::BarometricPressure{987.5f});
    state.pressureAltQnh.store(GATAS::PressureAltQnh{450, 1013.0f});
    const auto altitudeSnapshot = reader.pressureAltQnh.load();
    const auto pressureSnapshot = reader.barometricPressure.load();
    REQUIRE(pressureSnapshot.pressurehPa == 987.5f);
    REQUIRE(altitudeSnapshot.pressureAlt == 450);
    REQUIRE(altitudeSnapshot.qnh == 1013.0f);
    REQUIRE(reader.location.load().timestamp == 1234);

    location.timestamp = 5678;
    location.lat = 53.0f;
    state.location.store(location);
    REQUIRE(snapshot.timestamp == 1234);
    REQUIRE(snapshot.lat == 52.0f);
    REQUIRE(snapshot.lon == 5.0f);
    REQUIRE(snapshot.ellipseHeight == 450);
    REQUIRE(reader.location.load().timestamp == 5678);
    REQUIRE(reader.location.load().lat == 53.0f);
    REQUIRE(reader.barometricPressure.load().pressurehPa == 987.5f);

    state.barometricPressure.store(GATAS::BarometricPressure{});
    state.pressureAltQnh.store(GATAS::PressureAltQnh{});
    REQUIRE(reader.pressureAltQnh.load().pressureAlt == GATAS::INVALID_BARO_ALTITUDE);
    REQUIRE(altitudeSnapshot.pressureAlt == 450);
}

TEST_CASE("Ownship pressure altitude follows all five priorities", "[ownshipstate]")
{
    GATAS::OwnshipState state;
    state.init(spin_lock_instance(0));
    GATAS::OwnshipPositionInfo position{};
    position.timestamp = 1;
    position.ellipseHeight = 1050;
    position.geoidSeparation = 50;
    state.location.store(position);

    SECTION("Measured equipment altitude wins without QNH")
    {
        state.pressureAltQnh.store({-50, GATAS::INVALID_QNH, GATAS::PressureSource::PressureSensor});
        state.barometricPressure.store({898.7628f});
        REQUIRE(state.calculatePressureAltitude().value() == -50.0f);
    }
    SECTION("Ambient pressure wins over other received altitudes")
    {
        for (const auto source : {GATAS::PressureSource::Calculated})
        {
            state.pressureAltQnh.store({2000, 1013.25f, source});
            state.barometricPressure.store({898.7628f});
            REQUIRE(state.calculatePressureAltitude().value() == Catch::Approx(1000.0f).margin(0.1f));
            state.barometricPressure.store({});
            REQUIRE(state.calculatePressureAltitude().value() == 2000.0f);
        }
    }
    SECTION("Missing equipment altitude falls through to ambient pressure")
    {
        state.pressureAltQnh.store({GATAS::INVALID_BARO_ALTITUDE, 1013.25f, GATAS::PressureSource::PressureSensor});
        state.barometricPressure.store({1013.25f});
        REQUIRE(state.calculatePressureAltitude().value() == Catch::Approx(0.0f).margin(0.01f));
        state.barometricPressure.store({1025.0f});
        REQUIRE(state.calculatePressureAltitude().value() == Catch::Approx(-97.4f).margin(0.1f));
    }
    SECTION("Invalid ambient pressure falls through to received altitude without QNH")
    {
        state.pressureAltQnh.store({0, GATAS::INVALID_QNH, GATAS::PressureSource::Calculated});
        for (const float pressure : {0.0f, -1.0f, std::numeric_limits<float>::quiet_NaN(),
                                     std::numeric_limits<float>::infinity()})
        {
            state.barometricPressure.store({pressure});
            REQUIRE(state.calculatePressureAltitude().value() == 0.0f);
        }
    }
    SECTION("GPS MSL and QNH give an estimate when pressure altitude is missing")
    {
        state.pressureAltQnh.store({GATAS::INVALID_BARO_ALTITUDE, 1013.25f});
        REQUIRE(state.calculatePressureAltitude().value() == Catch::Approx(1000.0f).margin(0.1f));
        state.pressureAltQnh.store({GATAS::INVALID_BARO_ALTITUDE, 1000.0f});
        REQUIRE(state.calculatePressureAltitude().value() == Catch::Approx(1108.4f).margin(0.2f));
        state.pressureAltQnh.store({GATAS::INVALID_BARO_ALTITUDE, 1025.0f});
        REQUIRE(state.calculatePressureAltitude().value() == Catch::Approx(904.8f).margin(0.2f));
    }
    SECTION("GPS alone or invalid QNH leaves pressure altitude unavailable")
    {
        for (const float qnh : {GATAS::INVALID_QNH, 0.0f, std::numeric_limits<float>::quiet_NaN(),
                                std::numeric_limits<float>::infinity()})
        {
            state.pressureAltQnh.store({GATAS::INVALID_BARO_ALTITUDE, qnh});
            REQUIRE_FALSE(state.calculatePressureAltitude().has_value());
        }
    }
    SECTION("QNH without a stored position leaves pressure altitude unavailable")
    {
        state.pressureAltQnh.store({GATAS::INVALID_BARO_ALTITUDE, 1013.25f});
        state.location.store({});
        REQUIRE_FALSE(state.calculatePressureAltitude().has_value());
        position.ellipseHeight = 44380;
        state.location.store(position);
        REQUIRE_FALSE(state.calculatePressureAltitude().has_value());
    }
}
