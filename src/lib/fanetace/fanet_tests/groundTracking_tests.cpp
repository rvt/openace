
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "../fanet/fanet.hpp"
#include "etl/vector.h"
#include "helpers.hpp"

using namespace FANET;



TEST_CASE("GroundTrackingPayload Default Constructor", "[GroundTrackingPayload]") {
    GroundTrackingPayload payload;

    REQUIRE(payload.type() == Header::MessageType::GROUND_TRACKING);
    REQUIRE(payload.latitude() == 0);
    REQUIRE(payload.longitude() == 0);
    REQUIRE(payload.tracking() == false);
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

TEST_CASE("GroundTrackingPayload groundType ", "[single-file]")
{
    GroundTrackingPayload payload;
    payload.groundType(GroundTrackingPayload::TrackingType::BIKE);
    REQUIRE(payload.groundType() == GroundTrackingPayload::TrackingType::BIKE);
}

TEST_CASE("GroundTrackingPayload serialize/deserialize empty", "[single-file]")
{
    GroundTrackingPayload payload;
    auto result = createRadioPacket(payload);
    REQUIRE(result == makeVector({0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, }));
}


TEST_CASE("GroundTrackingPayload serialize/deserialize Type", "[single-file]")
{
    GroundTrackingPayload payload;
    payload.groundType(GroundTrackingPayload::TrackingType::NEED_A_RIDE); // -> 0x08 << 4 0x80
    auto result = createRadioPacket(payload);
    REQUIRE(result == makeVector({0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, }));

    auto reader = createReader(result);
    auto received=GroundTrackingPayload::deserialize(reader);
    REQUIRE(received.groundType() == GroundTrackingPayload::TrackingType::NEED_A_RIDE);
}


TEST_CASE("GroundTrackingPayload serialize/deserialize tracking", "[single-file]")
{
    GroundTrackingPayload payload;
    payload.tracking(true);
    auto result = createRadioPacket(payload);
    REQUIRE(result == makeVector({0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01}));   

    auto reader = createReader(result);
    auto received=GroundTrackingPayload::deserialize(reader);
    REQUIRE(received.tracking() == true);   
}


TEST_CASE("GroundTrackingPayload serialize/deserialize lat/long", "[single-file]")
{
    GroundTrackingPayload payload;
    payload.latitude(52.4123f);
    payload.longitude(-24.6123f);
    // printf("lat:%X lon:%X\n", (int32_t)roundf(52.4123f * 93206.0f), (int32_t)roundf(-24.6123f * 46603.0f)); // lat:4A8A95 lon:FFEE7F81
    auto result = createRadioPacket(payload);
    REQUIRE(result == makeVector({0x95, 0x8A, 0x4A, 0x81, 0x7F, 0xEE, 0x00 }));    

    auto reader = createReader(result);
    auto received=GroundTrackingPayload::deserialize(reader);
    REQUIRE(received.longitude() == Catch::Approx(-24.6123f).margin(0.1));
    REQUIRE(received.latitude() == Catch::Approx(52.4123f).margin(0.1));
}
