
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
    REQUIRE(payload.groundTrack() == 0);
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

    tp.altitude(2046);
    REQUIRE(tp.altitude() == 2046);
    tp.altitude(2047);
    REQUIRE(tp.altitude() == 2047);

    tp.altitude(5677);
    REQUIRE(tp.altitude() == 5676);
    tp.altitude(5678);
    REQUIRE(tp.altitude() == 5680);
    tp.altitude(5681);
    REQUIRE(tp.altitude() == 5680);
    tp.altitude(5682);
    REQUIRE(tp.altitude() == 5684);

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
    REQUIRE(tp.climbRate() == Catch::Approx(0).margin(0.1));

    tp.climbRate(6.2);
    REQUIRE(tp.climbRate() == Catch::Approx(6.2).margin(0.1));
 
    tp.climbRate(-6.2);
    REQUIRE(tp.climbRate() == Catch::Approx(-6.2).margin(0.1));

    tp.climbRate(16.8);
    REQUIRE(tp.climbRate() == Catch::Approx(16.8).margin(0.5));
 
    tp.climbRate(-16.8);
    REQUIRE(tp.climbRate() == Catch::Approx(-16.8).margin(0.5));

    tp.climbRate(31.5);
    REQUIRE(tp.climbRate() == Catch::Approx(31.5).margin(0.5));

    tp.climbRate(-31.5);
    REQUIRE(tp.climbRate() == Catch::Approx(-31.5).margin(0.5));

    tp.climbRate(100.0f);
    REQUIRE(tp.climbRate() == Catch::Approx(31.5).margin(0.5));

    tp.climbRate(-100.0f);
    REQUIRE(tp.climbRate() == Catch::Approx(-31.5).margin(0.5));
}

TEST_CASE("TrackingPayload heading ", "[single-file]")
{
    TrackingPayload tp;
    REQUIRE(tp.groundTrack() == Catch::Approx(0).margin(1.4));

    tp.groundTrack(360.0f);
    REQUIRE(tp.groundTrack() == Catch::Approx(0.f).margin(1.4));

    tp.groundTrack(-10.f);
    REQUIRE(tp.groundTrack() == Catch::Approx(350).margin(1.4));
 
    tp.groundTrack(127.f);
    REQUIRE(tp.groundTrack() == Catch::Approx(127).margin(1.4));

    tp.groundTrack(380.f);
    REQUIRE(tp.groundTrack() == Catch::Approx(20.0f).margin(1.4));

}