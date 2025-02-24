
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "../fanet/fanet.hpp"
#include "etl/vector.h"
#include "helpers.hpp"

using namespace FANET;



TEST_CASE("TrackingPayload Default Constructor", "[TrackingPayload]") {
    TrackingPayload payload;

    REQUIRE(payload.type() == Header::MessageType::TRACKING);
    REQUIRE(payload.latitude() == 0);
    REQUIRE(payload.longitude() == 0);
    REQUIRE(payload.altitude() == 0);
    REQUIRE(payload.aircraftType() == TrackingPayload::AircraftType::OTHER);
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
    payload.aircraftType(TrackingPayload::AircraftType::GLIDER);
    REQUIRE(payload.aircraftType() == TrackingPayload::AircraftType::GLIDER);
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
TEST_CASE("TrackingPayload turnRate ", "[single-file]")
{
    TrackingPayload payload;
    payload.turnRate(6.2);
    REQUIRE(payload.turnRate() == Catch::Approx(6.2).margin(0.5));

    payload.turnRate(-6.2);
    REQUIRE(payload.turnRate() == Catch::Approx(-6.2).margin(0.5));

    payload.turnRate(33.5);
    REQUIRE(payload.turnRate() == Catch::Approx(33.5).margin(0.5));

    payload.turnRate(-33.5);
    REQUIRE(payload.turnRate() == Catch::Approx(-33.5).margin(0.5));

    payload.turnRate(100);
    REQUIRE(payload.turnRate() == Catch::Approx(64).margin(0.5));

    payload.turnRate(-100);
    REQUIRE(payload.turnRate() == Catch::Approx(-64).margin(0.5));
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

TEST_CASE("TrackingPayload serialize/deserialize empty", "[single-file]")
{
    TrackingPayload payload;
    auto result = createRadioPacket(payload);
    REQUIRE(result == make_etl_vector({0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}));
}

TEST_CASE("TrackingPayload serialize/deserialize altitude", "[single-file]")
{
    TrackingPayload payload;
    payload.altitude(5000);
    // printf("ALT:%X\n", altitude_Origional(5000)); // -> 4E2    
    auto result = createRadioPacket(payload);
    REQUIRE(result == make_etl_vector({0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xE2, 0x0C, 0x00, 0x00, 0x00, 0x00, }));
    
    auto reader = createReader(result);
    auto received=TrackingPayload::deserialize(reader);
    REQUIRE(received.altitude() == Catch::Approx(5000).margin(0.1));
}

TEST_CASE("TrackingPayload serialize/deserialize aircraftType", "[single-file]")
{
    TrackingPayload payload;
    payload.aircraftType(TrackingPayload::AircraftType::GLIDER); // -> 0x04 << 4 
    auto result = createRadioPacket(payload);
    REQUIRE(result == make_etl_vector({0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00 }));

    auto reader = createReader(result);
    auto received=TrackingPayload::deserialize(reader);
    REQUIRE(received.aircraftType() == TrackingPayload::AircraftType::GLIDER);
}

TEST_CASE("TrackingPayload serialize/deserialize climbRate", "[single-file]")
{
    TrackingPayload payload;

    payload.climbRate(5.5f); // -> 0x49
    //printf("Climb:0x%X\n", climbRate_Origional(5.5f)); // -> 0x37
    auto result = createRadioPacket(payload);
    REQUIRE(result == make_etl_vector({0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x37, 0x00, 0x00}));    
    
    auto reader = createReader(result);
    auto received=TrackingPayload::deserialize(reader);
    REQUIRE(received.climbRate() == Catch::Approx(5.5).margin(0.1));

    payload.climbRate(-5.5f); // -> 0x49
    //printf("Climb:0x%X\n", climbRate_Origional(-5.5f)); // -> 0xC9 ^ 0x80==> 0x49 (0x080 removes scaling bit)
    result = createRadioPacket(payload);
    REQUIRE(result == make_etl_vector({0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x49, 0x00, 0x00}));
    
    auto reader2 = createReader(result);
    received=TrackingPayload::deserialize(reader2);
    REQUIRE(received.climbRate() == Catch::Approx(-5.5).margin(0.1));
}

TEST_CASE("TrackingPayload serialize/deserialize groundTrack", "[single-file]")
{
    TrackingPayload payload;
    payload.groundTrack(100); // -> 100 * 256 / 360.f = 0X47
    auto result = createRadioPacket(payload);
    REQUIRE(result == make_etl_vector({0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x47, 0x00,}));  

    auto reader = createReader(result);
    auto received=TrackingPayload::deserialize(reader);
    REQUIRE(received.groundTrack() == Catch::Approx(100).margin(1.4));  
}

TEST_CASE("TrackingPayload serialize/deserialize speed", "[single-file]")
{
    TrackingPayload payload;
    payload.speed(234.f);
    // printf("Speed:%X\n", speed_Origional(234.f)); // -> 0x5E + scale bit
    auto result = createRadioPacket(payload);
    REQUIRE(result == make_etl_vector({0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xDE, 0x00, 0x00, 0x00,}));    

    auto reader = createReader(result);
    auto received=TrackingPayload::deserialize(reader);
    REQUIRE(received.speed() == Catch::Approx(234.f).margin(2));  
}

TEST_CASE("TrackingPayload serialize/deserialize tracking", "[single-file]")
{
    TrackingPayload payload;
    payload.tracking(true);
    auto result = createRadioPacket(payload);
    REQUIRE(result == make_etl_vector({0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00,}));   

    auto reader = createReader(result);
    auto received=TrackingPayload::deserialize(reader);
    REQUIRE(received.tracking() == true);   
}

TEST_CASE("TrackingPayload serialize/deserialize turnrate", "[single-file]")
{
    TrackingPayload payload;
    payload.turnRate(14.4f);
    // printf("turnRate:%X\n", turnRate_Origional(14.4f)); // -> 0x3A
    auto result = createRadioPacket(payload);
    REQUIRE(result == make_etl_vector({0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3A,}));    

    auto reader = createReader(result);
    auto received=TrackingPayload::deserialize(reader);
    REQUIRE(received.turnRate() == Catch::Approx(14.4f).margin(0.2));  

    payload.turnRate(-14.4f);
    // printf("turnRate:%X\n", turnRate_Origional(-14.4f)); // -> 0xC6 ^ 0x80 ==> 0x46
    result = createRadioPacket(payload);
    REQUIRE(result == make_etl_vector({0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46,}));    

    auto reader2 = createReader(result);
    auto received2=TrackingPayload::deserialize(reader2);
    REQUIRE(received2.turnRate() == Catch::Approx(-14.4f).margin(0.2));  
}

TEST_CASE("TrackingPayload serialize/deserialize lat/long", "[single-file]")
{
    TrackingPayload payload;
    payload.latitude(52.4123f);
    payload.longitude(-24.6123f);
    // printf("lat:%X lon:%X\n", (int32_t)roundf(52.4123f * 93206.0f), (int32_t)roundf(-24.6123f * 46603.0f)); // lat:4A8A95 lon:FFEE7F81
    auto result = createRadioPacket(payload);
    REQUIRE(result == make_etl_vector({0x95, 0x8A, 0x4A, 0x81, 0x7F, 0xEE, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, }));    

    auto reader = createReader(result);
    auto received=TrackingPayload::deserialize(reader);
    REQUIRE(received.longitude() == Catch::Approx(-24.6123f).margin(0.1));
    REQUIRE(received.latitude() == Catch::Approx(52.4123f).margin(0.1));
}
