
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#define private public

#include "../include/flarm/flarm2024packet.hpp"

#include <stdio.h>

// #include "flarm_utils.hpp"
// #include "flarm2024.hpp"
// #include "ace/ognconv.hpp"

Flarm2024Packet packet;

TEST_CASE("selfCheck", "[single-file]")
{
    REQUIRE(0 == Flarm2024Packet::selfCheck());
}

TEST_CASE("Altitude", "[single-file]")
{
    packet.aircraftId(12);
    REQUIRE(12 == packet.aircraftId());
    packet.aircraftId(0);
    REQUIRE(0 == packet.aircraftId());

    packet.aircraftId(0x12234567);
    REQUIRE(0x00234567 == packet.aircraftId());
}

TEST_CASE("messageType", "[single-file]")
{
    packet.messageType(15);
    REQUIRE(15 == packet.messageType());
    packet.messageType(2);
    REQUIRE(2 == packet.messageType());
}

TEST_CASE("addressType", "[single-file]")
{
    packet.addressType(2);
    REQUIRE(2 == packet.addressType());
    packet.addressType(3);
    REQUIRE(0 == packet.addressType());
}

TEST_CASE("stealth", "[single-file]")
{
    packet.stealth(true);
    REQUIRE(true == packet.stealth());
    packet.stealth(false);
    REQUIRE(false == packet.stealth());
}

TEST_CASE("noTrack", "[single-file]")
{
    packet.noTrack(true);
    REQUIRE(true == packet.noTrack());
    packet.noTrack(false);
    REQUIRE(false == packet.noTrack());
}

TEST_CASE("epochSeconds", "[single-file]")
{
    packet.epochSeconds(1751731311);
    REQUIRE(15 == packet.epochSecondsLSB());

    packet.epochSeconds(1751731311 + 3);
    REQUIRE(2 == packet.epochSecondsLSB());
}

TEST_CASE("aircraftType", "[single-file]")
{
    packet.aircraftType(12);
    REQUIRE(12 == packet.aircraftType());

    packet.aircraftType(17);
    REQUIRE(15 == packet.aircraftType());
}

TEST_CASE("altitude", "[single-file]")
{
    packet.altitude(8000);
    REQUIRE(8000 == packet.altitude());

    packet.altitude(3000);
    REQUIRE(3000 == packet.altitude());

    packet.altitude(-900);
    REQUIRE(-900 == packet.altitude());

    packet.altitude(15000);
    REQUIRE(11286 == packet.altitude());
}

TEST_CASE("position", "[single-file]")
{
    for (float lat : {-52, 52})
    {
        for (float lon : {-6, 6})
        {
            packet.setPosition(lat, lon);
            auto pos = packet.getPosition(lat + 0.3, lon + 0.7);
            REQUIRE(Catch::Approx(lat).margin(0.00001) == pos.latitude);
            REQUIRE(Catch::Approx(lon).margin(0.00001) == pos.longitude);
        }
    }
}

TEST_CASE("turnRate", "[single-file]")
{
    packet.turnRate(12.0f);
    REQUIRE(Catch::Approx(12).margin(0.00001) == packet.turnRate());

    packet.turnRate(-5.0f);
    REQUIRE(Catch::Approx(-5).margin(0.00001) == packet.turnRate());
}

TEST_CASE("groundSpeed", "[single-file]")
{
    packet.groundSpeed(80.2f);
    REQUIRE(Catch::Approx(80.2f).margin(0.2) == packet.groundSpeed());
    packet.groundSpeed(30.2f);
    REQUIRE(Catch::Approx(30.2f).margin(0.2) == packet.groundSpeed());
    packet.groundSpeed(-2);
    REQUIRE(Catch::Approx(0.0f).margin(0.00001) == packet.groundSpeed());
}

TEST_CASE("verticalSpeed", "[single-file]")
{
    packet.verticalSpeed(12.2f);
    REQUIRE(Catch::Approx(12.2f).margin(0.00001) == packet.verticalSpeed());
    packet.verticalSpeed(-5.5f);
    REQUIRE(Catch::Approx(-5.5f).margin(0.00001) == packet.verticalSpeed());
}

TEST_CASE("groundTrack", "[single-file]")
{
    packet.groundTrack(5.5);
    REQUIRE(5.5f == packet.groundTrack());
    REQUIRE(Catch::Approx(5.5f).margin(0.5) == packet.groundTrack());

    packet.groundTrack(-5.5f);
    REQUIRE(Catch::Approx(354.5f).margin(0.5) == packet.groundTrack());

    packet.groundTrack(181.6f);
    REQUIRE(Catch::Approx(181.6f).margin(0.5) == packet.groundTrack());
}

TEST_CASE("movementStatus", "[single-file]")
{
    packet.movementStatus(2);
    REQUIRE(2 == packet.movementStatus());

    packet.movementStatus(8);
    REQUIRE(3 == packet.movementStatus());
}

TEST_CASE("packet.enscale/packet.descale unsigned", "[single-file]")
{
    REQUIRE(packet.descale<12, 3, false>(packet.enscale<12, 3, false>(1)) == 1);
    REQUIRE(packet.descale<12, 3, false>(packet.enscale<12, 3, false>(12)) == 12);
    REQUIRE(packet.descale<12, 3, false>(packet.enscale<12, 3, false>(123)) == 123);
    REQUIRE(packet.descale<12, 3, false>(packet.enscale<12, 3, false>(1234)) == 1234);
    REQUIRE(packet.descale<12, 3, false>(packet.enscale<12, 3, false>(12345)) == 12344);
    REQUIRE(packet.descale<12, 3, false>(packet.enscale<12, 3, false>(123456)) == 123456);

    // Full scale
    REQUIRE(packet.descale<16, 0, false>(packet.enscale<16, 0, false>(65535)) == 65535);
    REQUIRE(packet.descale<16, 0, true>(packet.enscale<16, 0, true>(-32768)) == -32768);

    // half scale
    REQUIRE(packet.descale<8, 8, false>(packet.enscale<8, 8, false>(256)) == 256);
    REQUIRE(packet.descale<8, 8, true>(packet.enscale<8, 8, true>(-256)) == -256);

    REQUIRE(packet.descale<8, 8, false>(packet.enscale<8, 8, false>(32768)) == 32768);
    REQUIRE(packet.descale<8, 8, false>(packet.enscale<8, 8, false>(32640)) == 32640);
    REQUIRE(packet.descale<8, 8, true>(packet.enscale<8, 8, true>(32642)) == 32640);
    REQUIRE(packet.descale<8, 8, true>(packet.enscale<8, 8, true>(-32640)) == -32640);
    REQUIRE(packet.descale<8, 8, true>(packet.enscale<8, 8, true>(32642)) == 32640);
    REQUIRE(packet.descale<8, 8, true>(packet.enscale<8, 8, true>(65535)) == 65280);
    REQUIRE(packet.descale<8, 8, true>(packet.enscale<8, 8, true>(-65535)) == -65280);

    // 1 bit
    REQUIRE(packet.descale<1, 0, false>(packet.enscale<1, 0, false>(1)) == 1);
    REQUIRE(packet.descale<1, 0, true>(packet.enscale<1, 0, true>(-1)) == -1);
    REQUIRE(packet.descale<1, 0, false>(packet.enscale<1, 0, false>(0)) == 0);
    REQUIRE(packet.descale<1, 0, true>(packet.enscale<1, 0, true>(0)) == 0);
}

TEST_CASE("lonDivisor", "[single-file]")
{
    auto testData = etl::make_array<int>(806, 299, 152, 105, 82, 67, 61, 56, 52, 52, 52, 56, 61, 67, 82, 105, 152, 299, 806);
    int c = 0;
    int lat = -90;
    do
    {
        REQUIRE(packet.lonDivisor(lat) == testData[c]);
        lat += +10;
        c++;
    } while (lat <= 90);
}

TEST_CASE("bteaAndScramble test for 100 seconds", "[single-file]")
{
    const uint32_t org[] = {0x12345678, 0x12345678, 0x12345678, 0x12345678, 0x12345678, 0x12345678, 0xabcd5678};
    uint32_t work[] = {0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000};

    uint32_t base = 1720119032;
    for (int i = 0; i < 100; i++)
    {
        uint32_t epoch = base + i;
        memcpy(work, org, sizeof(org));

        packet.scramble(work, epoch);
        packet.bteaEncode(work + 2);

        packet.bteaDecode(work + 2);
        packet.scramble(work, epoch);
        REQUIRE(memcmp(work, org, sizeof(org)) == 0);
    }

    // print_buffer_hex_uint32(work, 7);
    REQUIRE((work[6] & 0xFFFF0000) == 0xabcd0000);
}