
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

// #define private public

#include "../fanet/fanet_common.hpp"
#include "etl/vector.h"


constexpr uint32_t OUT_OF_ADAPTIVE_RANGE = 200000;
using namespace FANET;

void dumpHex(const etl::ivector<uint8_t> &buffer) {
    for (uint8_t byte : buffer) {
        printf("0x%02X, ", byte); // Print each byte in 2-digit uppercase hex
    }
    printf("\n"); // Newline for formatting
}

TEST_CASE("TrackingPayload Default Constructor", "[TrackingPayload]") {
    TrackingPayload payload;

    REQUIRE(payload.latitude() == 0);
    REQUIRE(payload.longitude() == 0);
    REQUIRE(payload.altitude() == 0);
    REQUIRE(payload.aircraftType() == AircraftType::OTHER);
    REQUIRE(payload.tracking() == false);
    REQUIRE(payload.speed() == 0);
    REQUIRE(payload.climbRate() == 0);
    REQUIRE(payload.heading() == 0);
    REQUIRE(payload.turnRate() == 0);
}

TEST_CASE("TrackingPayload Latitude ", "[single-file]")
{
    TrackingPayload tp;
    REQUIRE(tp.latitude() == Catch::Approx(0.0).margin(0.00001));
    tp.latitude(56.95812f);
    REQUIRE(tp.latitude() == Catch::Approx(56.95812f).margin(0.00001));
    tp.latitude(-56.18748);
    REQUIRE(tp.latitude() == Catch::Approx(-56.18748f).margin(0.00001));
    tp.latitude(-91);
    REQUIRE(tp.latitude() == Catch::Approx(-90).margin(0.00001));
    tp.latitude(91);
    REQUIRE(tp.latitude() == Catch::Approx(90).margin(0.00001));
}

TEST_CASE("TrackingPayload Longitude ", "[single-file]")
{
    TrackingPayload tp;
    REQUIRE(tp.longitude() == Catch::Approx(0.0).margin(0.00002));
    tp.longitude(160.54197);
    REQUIRE(tp.longitude() == Catch::Approx(160.54197).margin(0.00002));
    tp.longitude(-126.74510);
    REQUIRE(tp.longitude() == Catch::Approx(-126.74510).margin(0.00002));
    tp.longitude(-181);
    REQUIRE(tp.longitude() == Catch::Approx(-180).margin(0.00002));
    tp.longitude(181);
    REQUIRE(tp.longitude() == Catch::Approx(180).margin(0.00002));
}

TEST_CASE("TrackingPayload altitude ", "[single-file]")
{
    TrackingPayload tp;
    REQUIRE(tp.altitude() == 0);

    tp.altitude(1235);
    REQUIRE(tp.altitude() == 1235);

    tp.altitude(2047);
    REQUIRE(tp.altitude() == 2047);

    tp.altitude(5678);
    REQUIRE(tp.altitude() == 5680);
    tp.altitude(5679);
    REQUIRE(tp.altitude() == 5680);
    tp.altitude(5680);
    REQUIRE(tp.altitude() == 5680);

    tp.altitude(-100);
    REQUIRE(tp.altitude() == 0);
    tp.altitude(10000);
    REQUIRE(tp.altitude() == 8188);
}

TEST_CASE("TrackingPayload tracking ", "[single-file]")
{
    TrackingPayload tp;
    tp.tracking(true);
    REQUIRE(tp.tracking() == true);
}

TEST_CASE("TrackingPayload aircrafttype ", "[single-file]")
{
    TrackingPayload tp;
    tp.aircraftType(AircraftType::GLIDER);
    REQUIRE(tp.aircraftType() == AircraftType::GLIDER);
}

TEST_CASE("TrackingPayload speed ", "[single-file]")
{
    TrackingPayload tp;
    tp.speed(0);
    REQUIRE(tp.speed() == Catch::Approx(0).margin(0.5));

    tp.speed(-1);
    REQUIRE(tp.speed() == Catch::Approx(0).margin(0.5));

    tp.speed(60.2);
    REQUIRE(tp.speed() == Catch::Approx(60).margin(0.5));

    tp.speed(128.8);
    REQUIRE(tp.speed() == Catch::Approx(128.8).margin(2.5));

    tp.speed(320);
    REQUIRE(tp.speed() == Catch::Approx(317.5).margin(2.5));
}

TEST_CASE("TrackingPayload climbRate ", "[single-file]")
{
    TrackingPayload tp;
    REQUIRE(tp.climbRate() == Catch::Approx(0).margin(0.5));

    tp.speed(6.2);
    REQUIRE(tp.climbRate() == Catch::Approx(6.2).margin(0.5));
}