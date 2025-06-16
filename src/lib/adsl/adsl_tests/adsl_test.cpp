
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#define private public

#include "pico/rand.h"
#include "pico/time.h"
#include "adsl.hpp"
#include "adsl_packet.hpp"


TEST_CASE( "hello", "[single-file]" )
{
    ADSL_Packet packet;
    printf("Size: %ld TxBytes: %d\n", sizeof(ADSL_Packet), ADSL_Packet::TotalTxBytes);
    REQUIRE( (sizeof(ADSL_Packet) == 27) );
    REQUIRE( (ADSL_Packet::TotalTxBytes == sizeof(ADSL_Packet) - 3) ); // ALLIGN is not send, thus - 2
    REQUIRE( (packet.length == 24) ); // length is the packet excluding itself
}

TEST_CASE( "Correct bit errors and get data", "[single-file]" )
{
    // TODO: Need to re-generate a new ADSL package
    // uint32_t frame[] = {0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000};
    // uint32_t err[] =   {0x00000000,0x00000001,0x00000001,0x00000001,0x00000000,0x00000000,0x00000000};
    // int result =  ADSL_Packet::Correct(((uint8_t*)frame)+1, ((uint8_t*)err)+1);
    // REQUIRE( result == 3 );

    // ADSL_Packet packet;
    // memcpy(&packet.header, frame, ADSL_Packet::TotalTxBytes);
    // REQUIRE( packet.checkCRC() == 0 );

    // packet.Descramble();
    // REQUIRE( packet.length == 0x18 );
    // REQUIRE( packet.getLatitude() == Catch::Approx(5.892681f) );
    // REQUIRE( packet.getLongitude() == Catch::Approx(2.736240) );
    // REQUIRE( packet.getTrack() == Catch::Approx(0.) );
    // REQUIRE( packet.getaltitudeGeoid() == 307 );
    // REQUIRE( packet.getGroundSpeed() == Catch::Approx(54.5f) );
    // REQUIRE( packet.getVerticalRate() == Catch::Approx(2.75f) );

    // REQUIRE( (packet.payloadIdent == 0x02) );
    // REQUIRE( (packet.addressMapping == 0x05) );
    // REQUIRE( (packet.address == 0xB8b8b8) );
    // REQUIRE( (packet.reserved1 == 0) );
    // REQUIRE( (packet.relay == 0) );
    // REQUIRE( (packet.flightState == 2) );
    // REQUIRE( (packet.aircraftCategory == 1) );
    // REQUIRE( (packet.emergencyStatus == ADSL_Packet::ES_NoEmergency) );
    // REQUIRE( (packet.timeStamp == 3) );
}

TEST_CASE( "Getters and Setters", "[single-file]" )
{
    ADSL_Packet packet;

    packet.setHeightHAE(1000.f);
    REQUIRE( packet.getHeightHAE() == Catch::Approx(1000.f) );

    packet.setLatitude(53.12345678f);
    REQUIRE( packet.getLatitude() == Catch::Approx(53.12345678f) );

    packet.setLatitude(-53.12345678f);
    REQUIRE( packet.getLatitude() == Catch::Approx(-53.12345678f) );

    packet.setLongitude(-4.87654321f);
    REQUIRE( packet.getLongitude() == Catch::Approx(-4.87654321f) );

    packet.setLongitude(4.87654321f);
    REQUIRE( packet.getLongitude() == Catch::Approx(4.87654321f) );

    packet.setGroundSpeed(12.5f);
    REQUIRE( packet.getGroundSpeed() == Catch::Approx(12.5f) );

    packet.setGroundSpeed(0.25f);
    REQUIRE( packet.getGroundSpeed() == Catch::Approx(0.25f) );

    packet.setGroundSpeed(0.15f);
    REQUIRE( packet.getGroundSpeed() == Catch::Approx(0.f) );

    packet.setVerticalRate(0.f);
    REQUIRE( packet.getVerticalRate() == Catch::Approx(0.f) );

    packet.setVerticalRate(0.125);
    REQUIRE( packet.getVerticalRate() == Catch::Approx(0.125f) );

    packet.setVerticalRate(-2.5f);
    REQUIRE( packet.getVerticalRate() == Catch::Approx(-2.5f) );

    packet.setVerticalRate(5.25f);
    REQUIRE( packet.getVerticalRate() == Catch::Approx(5.25f) );

    packet.verticalRate = (int16_t)0x000;
    REQUIRE( packet.getVerticalRate() == Catch::Approx(0.f) );

    packet.verticalRate = (int16_t)0x001;
    REQUIRE( packet.getVerticalRate() == Catch::Approx(0.125f) );  // Differs slightly from spec

    packet.verticalRate = 0x001;
    packet.verticalRate |= 0x100;
    REQUIRE( packet.getVerticalRate() == Catch::Approx(-0.125f) );  // Differs slightly from spec

    packet.verticalRate = (int16_t)0x048;
    REQUIRE( packet.getVerticalRate() == Catch::Approx(10.125f) );  // Differs slightly from spec

    packet.verticalRate = 0x048;
    packet.verticalRate |= 0x100;
    REQUIRE( packet.getVerticalRate() == Catch::Approx(-10.125f) );  // Differs slightly from spec

    packet.verticalRate = (int16_t)0x0ff;
    REQUIRE( packet.getVerticalRate() == Catch::Approx(119.5f) );  // Differs slightly from spec

    packet.verticalRate = 0x0ff;
    packet.verticalRate |= 0x100;
    REQUIRE( packet.getVerticalRate() == Catch::Approx(-119.5f) ); // Differs slightly from spec

    packet.setTrack(125.f);
    REQUIRE( packet.getTrack() == Catch::Approx(124.45312f) );

    packet.setTrack(0.f);
    REQUIRE( packet.getTrack() == Catch::Approx(0.f) );

    packet.setTrack(359.f);
    REQUIRE( packet.getTrack() == Catch::Approx(358.59375f) );
}