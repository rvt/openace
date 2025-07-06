
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#define private public

#include <stdio.h>

#include "flarm_utils.hpp"
#include "flarm2024.hpp"
#include "ace/ognconv.hpp"


TEST_CASE( "flarmV7Packet_t size", "[single-file]" )
{
    auto data1 = etl::make_array<uint32_t>(0xE2B3261B, 0x539C8EBD, 0x4641D5AD, 0xAA8D583A, 0xA996B330, 0xD43F1DD5);
    REQUIRE( flarmCalculateChecksum((uint8_t*)data1.data(), 24) == 50175);

    auto data2 = etl::make_array<uint32_t>(0xE2B3261B, 0x539C8EBD, 0x4641D5AD, 0xAA8D583A, 0xffffffff, 0xD43F1DD5);
    REQUIRE( flarmCalculateChecksum((uint8_t*)data2.data(), 24) == 20902);

    auto data3 = etl::make_array<uint32_t>(0xE2B3261B, 0x539C8EBD, 0x4641D5AD, 0xAA8D583A, 0xffffffff, 0xD43F1DD6);
    REQUIRE( flarmCalculateChecksum((uint8_t*)data3.data(), 24) == 51834);
}

TEST_CASE("Enscale/Descale signed", "[single-file]")
{
    REQUIRE(descale<12, 3, true>(enscale<12, 3, true>(12345)) == 12344);
    REQUIRE(descale<12, 3, true>(enscale<12, 3, true>(-12345)) == -12344);
}

TEST_CASE("Enscale/Descale unsigned", "[single-file]")
{

    REQUIRE(descale<12, 3, false>(enscale<12, 3, false>(1)) == 1);
    REQUIRE(descale<12, 3, false>(enscale<12, 3, false>(12)) == 12);
    REQUIRE(descale<12, 3, false>(enscale<12, 3, false>(123)) == 123);
    REQUIRE(descale<12, 3, false>(enscale<12, 3, false>(1234)) == 1234);
    REQUIRE(descale<12, 3, false>(enscale<12, 3, false>(12345)) == 12344);
    REQUIRE(descale<12, 3, false>(enscale<12, 3, false>(123456)) == 123456);

    // Full scale
    REQUIRE(descale<16, 0, false>(enscale<16, 0, false>(65535)) == 65535);
    REQUIRE(descale<16, 0, true>(enscale<16, 0, true>(-32768)) == -32768);

    // half scale
    REQUIRE(descale<8, 8, false>(enscale<8, 8, false>(256)) == 256);
    REQUIRE(descale<8, 8, true>(enscale<8, 8, true>(-256)) == -256);

    REQUIRE(descale<8, 8, false>(enscale<8, 8, false>(32768)) == 32768);
    REQUIRE(descale<8, 8, false>(enscale<8, 8, false>(32640)) == 32640);
    REQUIRE(descale<8, 8, true>(enscale<8, 8, true>(32642)) == 32640);
    REQUIRE(descale<8, 8, true>(enscale<8, 8, true>(-32640)) == -32640);
    REQUIRE(descale<8, 8, true>(enscale<8, 8, true>(32642)) == 32640);
    REQUIRE(descale<8, 8, true>(enscale<8, 8, true>(65535)) == 65280);
    REQUIRE(descale<8, 8, true>(enscale<8, 8, true>(-65535)) == -65280);

    // 1 bit
    REQUIRE(descale<1, 0, false>(enscale<1, 0, false>(1)) == 1);
    REQUIRE(descale<1, 0, true>(enscale<1, 0, true>(-1)) == -1);
    REQUIRE(descale<1, 0, false>(enscale<1, 0, false>(0)) == 0);
    REQUIRE(descale<1, 0, true>(enscale<1, 0, true>(0)) == 0);
}

TEST_CASE("lonDivisor", "[single-file]")
{
    auto testData = etl::make_array<int>(806, 299, 152, 105, 82, 67, 61, 56, 52, 52, 52, 56, 61, 67, 82, 105, 152, 299, 806);
    int c = 0;
    int lat = -90;
    do
    {
        REQUIRE( lonDivisor(lat) == testData[c] );
        lat += +10;
        c++;
    }
    while (lat <= 90);
}