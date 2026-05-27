#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "../ace/aircraftpathpredictor.hpp"

TEST_CASE("AircraftPathPredictor extrapolates straight flight", "[single-file]")
{
    AircraftPathPredictor<4> predictor;

    REQUIRE(predictor.update(1'000'000, 0x123456, 52.0f, 4.0f, 1000, 5.0f, 50.0f, 90, 0.0f, false));
    REQUIRE(predictor.size() == 1);

    GATAS::AircraftPositionInfo predicted;
    predicted.address = 0x123456;
    REQUIRE(predictor.extrapolatedPos(3'000'000, predicted));

    auto rel = CoreUtils::northEastDistance(52.0f, 4.0f, predicted.lat, predicted.lon);
    REQUIRE(rel.north == Catch::Approx(0.0f).margin(1.0f));
    REQUIRE(rel.east == Catch::Approx(100.0f).margin(1.0f));
    REQUIRE(predicted.ellipseHeight == 1010);
    REQUIRE(predicted.track == 90);
    REQUIRE(predicted.hTurnRate == Catch::Approx(0.0f).margin(0.01f));
}

TEST_CASE("AircraftPathPredictor uses provided turn rate for curved prediction", "[single-file]")
{
    AircraftPathPredictor<4> predictor;

    REQUIRE(predictor.update(10'000'000, 0xABCDEF, 52.0f, 4.0f, 1000, 2.0f, 50.0f, 0, 10.0f, true));

    GATAS::AircraftPositionInfo predicted;
    predicted.address = 0xABCDEF;
    REQUIRE(predictor.extrapolatedPos(12'000'000, predicted));

    auto rel = CoreUtils::northEastDistance(52.0f, 4.0f, predicted.lat, predicted.lon);
    REQUIRE(rel.north == Catch::Approx(97.98f).margin(1.0f));
    REQUIRE(rel.east == Catch::Approx(17.22f).margin(1.0f));
    REQUIRE(predicted.ellipseHeight == 1004);
    REQUIRE(predicted.track == 20);
    REQUIRE(predicted.hTurnRate == Catch::Approx(10.0f).margin(0.01f));
}

TEST_CASE("AircraftPathPredictor derives turn rate from recent history", "[single-file]")
{
    AircraftPathPredictor<4> predictor;

    REQUIRE(predictor.update(0, 0x654321, 52.0f, 4.0f, 1000, 0.0f, 50.0f, 350, 0.0f, false));
    REQUIRE(predictor.update(1'000'000, 0x654321, 52.00045f, 4.0f, 1000, 0.0f, 50.0f, 0, 0.0f, false));
    REQUIRE(predictor.update(2'000'000, 0x654321, 52.00089f, 4.00008f, 1000, 0.0f, 50.0f, 10, 0.0f, false));

    GATAS::AircraftPositionInfo predicted;
    predicted.address = 0x654321;
    REQUIRE(predictor.extrapolatedPos(3'000'000, predicted));

    REQUIRE(predicted.track == 20);
    REQUIRE(predicted.hTurnRate == Catch::Approx(10.0f).margin(0.01f));

    auto rel = CoreUtils::northEastDistance(52.00089f, 4.00008f, predicted.lat, predicted.lon);
    REQUIRE(rel.north > 45.0f);
    REQUIRE(rel.east > 10.0f);
}

TEST_CASE("AircraftPathPredictor rejects out-of-order updates and expires tracks", "[single-file]")
{
    AircraftPathPredictor<4> predictor;

    REQUIRE(predictor.update(2'000'000, 0x111111, 52.0f, 4.0f, 1000, 0.0f, 40.0f, 180, 0.0f, false));
    REQUIRE_FALSE(predictor.update(1'000'000, 0x111111, 52.0f, 4.0f, 1000, 0.0f, 40.0f, 180, 0.0f, false));

    GATAS::AircraftPositionInfo predicted;
    predicted.address = 0x111111;
    REQUIRE_FALSE(predictor.extrapolatedPos(1'500'000, predicted));
    REQUIRE(predictor.extrapolatedPos(2'500'000, predicted));

    predictor.maintenance(12'000'001);
    REQUIRE(predictor.size() == 0);
    predicted.address = 0x111111;
    REQUIRE_FALSE(predictor.extrapolatedPos(12'000'001, predicted));
}

TEST_CASE("AircraftPathPredictor clears stale turn rate when update cannot derive one", "[single-file]")
{
    AircraftPathPredictor<4> predictor;

    REQUIRE(predictor.update(0, 0x333333, 52.0f, 4.0f, 1000, 0.0f, 50.0f, 0, 8.0f, true));
    REQUIRE(predictor.update(1'000'000, 0x333333, 52.00045f, 4.0f, 1000, 0.0f, 1.0f, 0, 0.0f, false));

    GATAS::AircraftPositionInfo predicted;
    predicted.address = 0x333333;
    REQUIRE(predictor.extrapolatedPos(2'000'000, predicted));

    REQUIRE(predicted.hTurnRate == Catch::Approx(0.0f).margin(0.01f));
    REQUIRE(predicted.track == 0);

    auto rel = CoreUtils::northEastDistance(52.00045f, 4.0f, predicted.lat, predicted.lon);
    REQUIRE(rel.north == Catch::Approx(1.0f).margin(1.0f));
    REQUIRE(rel.east == Catch::Approx(0.0f).margin(1.0f));
}

TEST_CASE("AircraftPathPredictor treats exact expiry boundary consistently", "[single-file]")
{
    AircraftPathPredictor<4> predictor;

    REQUIRE(predictor.update(1'000'000, 0x444444, 52.0f, 4.0f, 1000, 0.0f, 30.0f, 45, 0.0f, false));

    GATAS::AircraftPositionInfo predicted;
    predicted.address = 0x444444;
    REQUIRE_FALSE(predictor.extrapolatedPos(11'000'000, predicted));

    predictor.maintenance(11'000'000);
    REQUIRE(predictor.size() == 0);
    predicted.address = 0x444444;
    REQUIRE_FALSE(predictor.extrapolatedPos(11'000'000, predicted));
}

TEST_CASE("AircraftPathPredictor invalidates stale relative fields on extrapolation", "[single-file]")
{
    AircraftPathPredictor<4> predictor;

    GATAS::AircraftPositionInfo input;
    input.timestamp = 2'000'000;
    input.address = 0x555555;
    input.lat = 52.0f;
    input.lon = 4.0f;
    input.ellipseHeight = 1000;
    input.verticalSpeed = 0.0f;
    input.groundSpeed = 25.0f;
    input.track = 180;
    input.hTurnRate = 0.0f;
    input.distanceFromOwn = 12345;
    input.relNorthFromOwn = 678;
    input.relEastFromOwn = -910;

    REQUIRE(predictor.update(input, false));

    GATAS::AircraftPositionInfo predicted;
    predicted.address = input.address;
    REQUIRE(predictor.extrapolatedPos(3'000'000, predicted));

    REQUIRE(predicted.distanceFromOwn == static_cast<uint32_t>(INT32_MIN));
    REQUIRE(predicted.relNorthFromOwn == INT32_MIN);
    REQUIRE(predicted.relEastFromOwn == INT32_MIN);
}

TEST_CASE("AircraftPathPredictor rejects update when track map is full", "[single-file]")
{
    AircraftPathPredictor<1> predictor;

    REQUIRE(predictor.update(1'000'000, 0x100001, 52.0f, 4.0f, 1000, 0.0f, 20.0f, 0, 0.0f, false));
    REQUIRE_FALSE(predictor.update(1'000'000, 0x100002, 52.1f, 4.1f, 1000, 0.0f, 20.0f, 0, 0.0f, false));
    REQUIRE(predictor.size() == 1);
}

TEST_CASE("AircraftPathPredictor replaces same-timestamp updates", "[single-file]")
{
    AircraftPathPredictor<4> predictor;

    REQUIRE(predictor.update(2'000'000, 0x200001, 52.0f, 4.0f, 1000, 0.0f, 10.0f, 0, 0.0f, false));
    REQUIRE(predictor.update(2'000'000, 0x200001, 52.001f, 4.001f, 1500, 3.0f, 30.0f, 90, 0.0f, false));

    GATAS::AircraftPositionInfo predicted;
    predicted.address = 0x200001;
    REQUIRE(predictor.extrapolatedPos(3'000'000, predicted));

    auto rel = CoreUtils::northEastDistance(52.001f, 4.001f, predicted.lat, predicted.lon);
    REQUIRE(rel.north == Catch::Approx(0.0f).margin(1.0f));
    REQUIRE(rel.east == Catch::Approx(30.0f).margin(1.0f));
    REQUIRE(predicted.ellipseHeight == 1503);
    REQUIRE(predicted.track == 90);
}

TEST_CASE("AircraftPathPredictor tracks multiple aircraft concurrently", "[single-file]")
{
    AircraftPathPredictor<4> predictor;

    REQUIRE(predictor.update(1'000'000, 0x300001, 52.0f, 4.0f, 1000, 0.0f, 20.0f, 0, 0.0f, false));
    REQUIRE(predictor.update(1'000'000, 0x300002, 53.0f, 5.0f, 2000, 0.0f, 40.0f, 180, 0.0f, false));

    GATAS::AircraftPositionInfo northbound;
    GATAS::AircraftPositionInfo southbound;
    northbound.address = 0x300001;
    southbound.address = 0x300002;
    REQUIRE(predictor.extrapolatedPos(2'000'000, northbound));
    REQUIRE(predictor.extrapolatedPos(2'000'000, southbound));

    auto relNorthbound = CoreUtils::northEastDistance(52.0f, 4.0f, northbound.lat, northbound.lon);
    auto relSouthbound = CoreUtils::northEastDistance(53.0f, 5.0f, southbound.lat, southbound.lon);
    REQUIRE(relNorthbound.north == Catch::Approx(20.0f).margin(1.0f));
    REQUIRE(relSouthbound.north == Catch::Approx(-40.0f).margin(1.0f));
    REQUIRE(predictor.size() == 2);
}

TEST_CASE("AircraftPathPredictor wraps longitude across the antimeridian", "[single-file]")
{
    AircraftPathPredictor<4> predictor;

    REQUIRE(predictor.update(1'000'000, 0x400001, 0.0f, 179.99995f, 1000, 0.0f, 20.0f, 90, 0.0f, false));

    GATAS::AircraftPositionInfo predicted;
    predicted.address = 0x400001;
    REQUIRE(predictor.extrapolatedPos(2'000'000, predicted));

    REQUIRE(predicted.lon < -179.0f);
}

TEST_CASE("AircraftPathPredictor clamps turn rate and normalizes track", "[single-file]")
{
    AircraftPathPredictor<4> predictor;

    REQUIRE(predictor.update(1'000'000, 0x500001, 52.0f, 4.0f, 1000, 0.0f, 50.0f, 370, 50.0f, true));

    GATAS::AircraftPositionInfo predicted;
    predicted.address = 0x500001;
    REQUIRE(predictor.extrapolatedPos(2'000'000, predicted));

    REQUIRE(predicted.hTurnRate == Catch::Approx(15.0f).margin(0.01f));
    REQUIRE(predicted.track == 25);
}
