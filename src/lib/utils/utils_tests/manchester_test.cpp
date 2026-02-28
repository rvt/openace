#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <stdio.h>

#include "manchester.hpp"
#include "etl/array.h"
#include "etl/span.h"
#include "testhelpers.h"

TEST_CASE("manchesterEncode Test", "[single-file]")
{
    uint8_t decode[] = {0x72, 0x4B};
    etl::array<uint8_t, 4> encoded;
    manchesterEncode(encoded.data(), decode, 2);
    REQUIRE(etl::make_array<uint8_t>(0x95, 0xA6, 0x9A, 0x65) == encoded);
}

TEST_CASE("manchesterDecode Test", "[single-file]")
{
    uint8_t encoded[] = {0x5A, 0x5A};
    etl::array<uint8_t, 1> decoded;
    manchesterDecode(decoded.data(), encoded, 2);
    REQUIRE(etl::make_array<uint8_t>(0xCC) == decoded);
}

TEST_CASE("manchesterDecodeInline with bit flip Test", "[single-file]")
{
    uint8_t encoded[] = {0x58, 0x5A};
    etl::array<uint8_t, 1> decoded;
    etl::array<uint8_t, 1> error;
    manchesterDecodeInline(decoded.data(), error.data(), 2);
    REQUIRE(etl::make_array<uint8_t>(0xF1) == error);
}

TEST_CASE("manchesterDecodeInline Test", "[single-file]")
{
    auto decoded=etl::make_array<uint8_t>(0x5A, 0x5A);
    etl::array<uint8_t, 1> error;
    manchesterDecodeInline(decoded.data(), error.data(), 2);
    REQUIRE(etl::make_array<uint8_t>(0xCC) == etl::make_array<uint8_t>(decoded[0]));
    REQUIRE(etl::make_array<uint8_t>(0x00) == error);
}

TEST_CASE("swapBytes16", "[single-file]")
{
    REQUIRE((swapBytes16(0x1234) == 0x3412));
    REQUIRE((swapBytes16(0xABCD) == 0xCDAB));
}
