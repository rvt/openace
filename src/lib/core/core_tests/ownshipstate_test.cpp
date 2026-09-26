#include <catch2/catch_test_macros.hpp>

#include "ace/ownshipstate.hpp"
#include "ace/coreutils.hpp"

TEST_CASE("Ownship state stores independent location and pressure snapshots", "[ownshipstate]")
{
    CoreUtils::init();
    GATAS::OwnshipState state;
    state.init(CoreUtils::sharedSpinLock());
    const auto &reader = state;

    REQUIRE(reader.location.load().timestamp == 0);
    REQUIRE(reader.barometricPressure.load().pressurehPa == 0.0f);
    REQUIRE(reader.barometricPressure.load().pressure_alt == GATAS::INVALID_BARO_ALTITUDE);
    REQUIRE(reader.barometricPressure.load().qnh == GATAS::INVALID_QNH);

    GATAS::OwnshipPositionInfo location{};
    location.timestamp = 1234;
    location.lat = 52.0f;
    location.lon = 5.0f;
    location.ellipseHeight = 450;
    state.location.store(location);
    const auto snapshot = reader.location.load();

    state.barometricPressure.store(GATAS::BarometricPressure{987.5f, 450, 1013.0f});
    const auto pressureSnapshot = reader.barometricPressure.load();
    REQUIRE(pressureSnapshot.pressurehPa == 987.5f);
    REQUIRE(pressureSnapshot.pressure_alt == 450);
    REQUIRE(pressureSnapshot.qnh == 1013.0f);
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
    REQUIRE(reader.barometricPressure.load().pressure_alt == GATAS::INVALID_BARO_ALTITUDE);
    REQUIRE(pressureSnapshot.pressure_alt == 450);
}
