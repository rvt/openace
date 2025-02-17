
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

    REQUIRE(payload.type() == MessageType::TRACKING);
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
    TrackingPayload payload;
    REQUIRE(payload.latitude() == Catch::Approx(0.0).margin(0.00001));
    payload.latitude(56.95812f);
    REQUIRE(payload.latitude() == Catch::Approx(56.95812f).margin(0.00001));
    payload.latitude(-56.18748);
    REQUIRE(payload.latitude() == Catch::Approx(-56.18748f).margin(0.00001));
    payload.latitude(-91);
    REQUIRE(payload.latitude() == Catch::Approx(-90).margin(0.00001));
    payload.latitude(91);
    REQUIRE(payload.latitude() == Catch::Approx(90).margin(0.00001));
}

TEST_CASE("TrackingPayload Longitude ", "[single-file]")
{
    TrackingPayload payload;
    REQUIRE(payload.longitude() == Catch::Approx(0.0).margin(0.00002));
    payload.longitude(160.54197);
    REQUIRE(payload.longitude() == Catch::Approx(160.54197).margin(0.00002));
    payload.longitude(-126.74510);
    REQUIRE(payload.longitude() == Catch::Approx(-126.74510).margin(0.00002));
    payload.longitude(-181);
    REQUIRE(payload.longitude() == Catch::Approx(-180).margin(0.00002));
    payload.longitude(181);
    REQUIRE(payload.longitude() == Catch::Approx(180).margin(0.00002));
}

TEST_CASE("TrackingPayload altitude ", "[single-file]")
{
    TrackingPayload payload;
    REQUIRE(payload.altitude() == 0);

    payload.altitude(2046);
    REQUIRE(payload.altitude() == 2046);
    payload.altitude(2047);
    REQUIRE(payload.altitude() == 2047);

    payload.altitude(5677);
    REQUIRE(payload.altitude() == 5676);
    payload.altitude(5678);
    REQUIRE(payload.altitude() == 5680);
    payload.altitude(5681);
    REQUIRE(payload.altitude() == 5680);
    payload.altitude(5682);
    REQUIRE(payload.altitude() == 5684);

    payload.altitude(-100);
    REQUIRE(payload.altitude() == 0);
    payload.altitude(10000);
    REQUIRE(payload.altitude() == 8188);
}

TEST_CASE("TrackingPayload tracking ", "[single-file]")
{
    TrackingPayload payload;
    payload.tracking(true);
    REQUIRE(payload.tracking() == true);
}

TEST_CASE("TrackingPayload aircrafttype ", "[single-file]")
{
    TrackingPayload payload;
    payload.aircraftType(AircraftType::GLIDER);
    REQUIRE(payload.aircraftType() == AircraftType::GLIDER);
}

TEST_CASE("TrackingPayload speed ", "[single-file]")
{
    TrackingPayload payload;
    payload.speed(0);
    REQUIRE(payload.speed() == Catch::Approx(0).margin(0.5));

    payload.speed(-1);
    REQUIRE(payload.speed() == Catch::Approx(0).margin(0.5));

    payload.speed(60.2);
    REQUIRE(payload.speed() == Catch::Approx(60).margin(0.5));

    payload.speed(128.8);
    REQUIRE(payload.speed() == Catch::Approx(128.8).margin(2.5));

    payload.speed(320);
    REQUIRE(payload.speed() == Catch::Approx(317.5).margin(2.5));
}

TEST_CASE("TrackingPayload climbRate ", "[single-file]")
{
    TrackingPayload payload;
    REQUIRE(payload.climbRate() == Catch::Approx(0).margin(0.1));

    payload.climbRate(6.2);
    REQUIRE(payload.climbRate() == Catch::Approx(6.2).margin(0.1));
 
    payload.climbRate(-6.2);
    REQUIRE(payload.climbRate() == Catch::Approx(-6.2).margin(0.1));

    payload.climbRate(16.8);
    REQUIRE(payload.climbRate() == Catch::Approx(16.8).margin(0.5));
 
    payload.climbRate(-16.8);
    REQUIRE(payload.climbRate() == Catch::Approx(-16.8).margin(0.5));

    payload.climbRate(31.5);
    REQUIRE(payload.climbRate() == Catch::Approx(31.5).margin(0.5));

    payload.climbRate(-31.5);
    REQUIRE(payload.climbRate() == Catch::Approx(-31.5).margin(0.5));

    payload.climbRate(100.0f);
    REQUIRE(payload.climbRate() == Catch::Approx(31.5).margin(0.5));

    payload.climbRate(-100.0f);
    REQUIRE(payload.climbRate() == Catch::Approx(-31.5).margin(0.5));
}

TEST_CASE("GroundTrackingPayload Default Constructor", "[TrackingPayload]") {
    GroundTrackingPayload payload;

    REQUIRE(payload.type() == MessageType::GROUND_TRACKING);
    REQUIRE(payload.latitude() == 0);
    REQUIRE(payload.longitude() == 0);
    REQUIRE(payload.tracking() == false);
    REQUIRE(payload.unk() == 0);
    REQUIRE(payload.groundType() == GroundTrackingType::OTHER);
}

TEST_CASE("GroundTrackingPayload Latitude ", "[single-file]")
{
    GroundTrackingPayload payload;
    REQUIRE(payload.latitude() == Catch::Approx(0.0).margin(0.00001));
    payload.latitude(56.95812f);
    REQUIRE(payload.latitude() == Catch::Approx(56.95812f).margin(0.00001));
    payload.latitude(-56.18748);
    REQUIRE(payload.latitude() == Catch::Approx(-56.18748f).margin(0.00001));
    payload.latitude(-91);
    REQUIRE(payload.latitude() == Catch::Approx(-90).margin(0.00001));
    payload.latitude(91);
    REQUIRE(payload.latitude() == Catch::Approx(90).margin(0.00001));
}

TEST_CASE("GroundTrackingPayload Longitude ", "[single-file]")
{
    GroundTrackingPayload payload;
    REQUIRE(payload.longitude() == Catch::Approx(0.0).margin(0.00002));
    payload.longitude(160.54197);
    REQUIRE(payload.longitude() == Catch::Approx(160.54197).margin(0.00002));
    payload.longitude(-126.74510);
    REQUIRE(payload.longitude() == Catch::Approx(-126.74510).margin(0.00002));
    payload.longitude(-181);
    REQUIRE(payload.longitude() == Catch::Approx(-180).margin(0.00002));
    payload.longitude(181);
    REQUIRE(payload.longitude() == Catch::Approx(180).margin(0.00002));
}

TEST_CASE("GroundTrackingPayload tracking ", "[single-file]")
{
    GroundTrackingPayload payload;
    payload.tracking(true);
    REQUIRE(payload.tracking() == true);
}

TEST_CASE("GroundTrackingPayload groundTrackingType ", "[single-file]")
{
    GroundTrackingPayload payload;
    payload.groundType(GroundTrackingType::DISTRESS_CALL);
    REQUIRE(payload.groundType() == GroundTrackingType::DISTRESS_CALL);
}


TEST_CASE("NamePayload", "[single-file]") {
    NamePayload<123> payload;

    REQUIRE(payload.type() == MessageType::NAME);
    REQUIRE(payload.name() == etl::string_view("")); 
    payload.name("Foo and Bar");
    REQUIRE(payload.name() == etl::string_view("Foo and Bar")); 
    payload.name("Only this one");
    REQUIRE(payload.name() == etl::string_view("Only this one")); 
}


TEST_CASE("MessagePayload", "[single-file]") {
    MessagePayload<123> payload;

    REQUIRE(payload.type() == MessageType::MESSAGE);
    REQUIRE(payload.subHeader() == 0); 
    REQUIRE(payload.message().size() == 0); 
    payload.subHeader(12);
    REQUIRE(payload.subHeader() == 12); 

    etl::vector<uint8_t, 12> message = {0x80, 0x12, 0x56, 0x34, 0x30, 0x98, 0x54, 0x76, 0x32, 0x54, 0x76, 0x98};
    payload.message(message);
    REQUIRE(payload.message() == message);
}
