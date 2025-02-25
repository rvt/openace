
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "helpers.hpp"

#include "../fanet/fanet.hpp"
#include "etl/vector.h"

using namespace FANET;

TEST_CASE("GroundTrackingPayload Default Constructor", "[TrackingPayload]")
{
    GroundTrackingPayload payload;

    // REQUIRE(payload.type() == MessageType::GROUND_TRACKING);
    REQUIRE(payload.latitude() == 0);
    REQUIRE(payload.longitude() == 0);
    REQUIRE(payload.tracking() == false);
    REQUIRE(payload.groundType() == GroundTrackingPayload::TrackingType::OTHER);
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

TEST_CASE("GroundTrackingPayload TrackingType ", "[single-file]")
{
    GroundTrackingPayload payload;
    payload.groundType(GroundTrackingPayload::TrackingType::DISTRESS_CALL);
    REQUIRE(payload.groundType() == GroundTrackingPayload::TrackingType::DISTRESS_CALL);
}

TEST_CASE("NamePayload", "[single-file]")
{
    NamePayload<123> payload;

    // REQUIRE(payload.type() == MessageType::NAME);
    REQUIRE(payload.name() == etl::string_view(""));
    payload.name("Foo and Bar");
    REQUIRE(payload.name() == etl::string_view("Foo and Bar"));
    payload.name("Only this one");
    REQUIRE(payload.name() == etl::string_view("Only this one"));
}

TEST_CASE("MessagePayload", "[single-file]")
{
    MessagePayload<123> payload;

    // REQUIRE(payload.type() == MessageType::MESSAGE);
    REQUIRE(payload.subHeader() == 0);
    REQUIRE(payload.message().size() == 0);
    payload.subHeader(12);
    REQUIRE(payload.subHeader() == 12);

    etl::vector<uint8_t, 12> message = {0x80, 0x12, 0x56, 0x34, 0x30, 0x98, 0x54, 0x76, 0x32, 0x54, 0x76, 0x98};
    payload.message(message);
    REQUIRE(payload.message() == message);
}
