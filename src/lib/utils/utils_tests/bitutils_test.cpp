
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "math.h"

#define private public

#include <stdio.h>

#include "EMA.hpp"
#include "encryption.hpp"
#include "bitutils.hpp"
#include "testhelpers.h"
#include "manchester.hpp"

constexpr float MS_TO_FTPMIN = 196.850394f; // meter/sec to feet/min
constexpr float DEG_TO_RADS = M_PI / 180.f; // degrees to radians

TEST_CASE("buffersParity8", "[single-file]")
{
    uint8_t buffer[4] = {0x01, 0x02, 0x04, 0x08};
    REQUIRE((buffersParity8(buffer, 4) == 0));
    buffer[3] = 0x09;
    REQUIRE((buffersParity8(buffer, 4) == 1));
    buffer[0] = 0x03;
    REQUIRE((buffersParity8(buffer, 4) == 0));
}

TEST_CASE("bitShift basic functionality (uint8_t)", "[bitShift]")
{
    SECTION("No shift")
    {
        std::array<uint8_t, 16> data = {
            0x12,0x34,0x56,0x78,
            0x9A,0xBC,0xDE,0xF0,
            0x0F,0x0F,0x0F,0x0F,
            0xFF,0xFF,0xFF,0xFF
        };

        bitShift<true>(data.data(), 16, 0);

        REQUIRE(data == std::array<uint8_t, 16>{
            0x12,0x34,0x56,0x78,
            0x9A,0xBC,0xDE,0xF0,
            0x0F,0x0F,0x0F,0x0F,
            0xFF,0xFF,0xFF,0xFF
        });
    }

    SECTION("Shift by 4 bits")
    {
        std::array<uint8_t, 12> data = {
            0x12,0x34,0x56,0x78,0x9A,0xBC,0xDE,0xF0,0xFF,0xFF,0xFF,0xFF
        };

        bitShift<true>(data.data(), 8, 4); // only first 8 bytes are logical data

        // Expected: 0x23456789 0xABCDEF00
        REQUIRE(data[0] == 0x23);
        REQUIRE(data[1] == 0x45);
        REQUIRE(data[2] == 0x67);
        REQUIRE(data[3] == 0x89);

        REQUIRE(data[4] == 0xAB);
        REQUIRE(data[5] == 0xCD);
        REQUIRE(data[6] == 0xEF);
        REQUIRE(data[7] == 0x00);
    }

    SECTION("Shift by 16 bits")
    {
        std::array<uint8_t, 8> data = {
            0x12,0x34,0x56,0x78,
            0x9A,0xBC,0xDE,0xF1
        };

        bitShift<true>(data.data(), 8, 16);

        // Expected: 0x56789ABC 0xDEF10000
        REQUIRE(data[0] == 0x56);
        REQUIRE(data[1] == 0x78);
        REQUIRE(data[2] == 0x9A);
        REQUIRE(data[3] == 0xBC);

        REQUIRE(data[4] == 0xDE);
        REQUIRE(data[5] == 0xF1);
        REQUIRE(data[6] == 0x00);
        REQUIRE(data[7] == 0x00);
    }

    SECTION("Shift by 32 bits (4 bytes)")
    {
        std::array<uint8_t, 12> data = {
            0x11,0x11,0x11,0x11,
            0x22,0x22,0x22,0x22,
            0x33,0x33,0x33,0x33
        };

        bitShift<true>(data.data(), 12, 32);

        REQUIRE(data[0] == 0x22);
        REQUIRE(data[1] == 0x22);
        REQUIRE(data[2] == 0x22);
        REQUIRE(data[3] == 0x22);

        REQUIRE(data[4] == 0x33);
        REQUIRE(data[5] == 0x33);
        REQUIRE(data[6] == 0x33);
        REQUIRE(data[7] == 0x33);
    }

    SECTION("Shift by 36 bits (4 bytes + 4 bits)")
    {
        std::array<uint8_t, 12> data = {
            0x11,0x11,0x11,0x11,
            0x22,0x22,0x22,0x22,
            0x33,0x33,0x33,0x33
        };

        bitShift<true>(data.data(), 12, 36);

        // Expected: 0x22222223 0x33333330
        REQUIRE(data[0] == 0x22);
        REQUIRE(data[1] == 0x22);
        REQUIRE(data[2] == 0x22);
        REQUIRE(data[3] == 0x23);

        REQUIRE(data[4] == 0x33);
        REQUIRE(data[5] == 0x33);
        REQUIRE(data[6] == 0x33);
        REQUIRE(data[7] == 0x30);
    }

    SECTION("Shift more than total bits")
    {
        std::array<uint8_t, 8> data = {
            0xAA,0xAA,0xAA,0xAA,
            0x55,0x55,0x55,0x55
        };

        bitShift<true>(data.data(), 8, 80); // 64 + 16 bits

        for (auto b : data)
            REQUIRE(b == 0x00);
    }
}


TEST_CASE("diffBits basic functionality", "[diffBits]")
{
    // Test data
    constexpr uint32_t Ref[3] = {0xAAAAAAAA, 0x12345678, 0xFFFFFFFF};
    constexpr uint32_t Data[3] = {0xAAAAAAAA, 0x12345679, 0xFFFFFFFE};
    constexpr uint32_t Mask[3] = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};

    SECTION("Unmasked diffBits")
    {
        uint8_t count = diffBits<3>(Data, Ref);
        // Differences:
        // 0x12345678 ^ 0x12345679 = 0x1 → 1 bit
        // 0xFFFFFFFF ^ 0xFFFFFFFE = 0x1 → 1 bit
        // Total = 2 bits
        REQUIRE(count == 2);
    }

    SECTION("Masked diffBits")
    {
        uint8_t count = diffBits<3>(Data, Ref, Mask);
        // Same as above, mask doesn't filter anything
        REQUIRE(count == 2);
    }

    SECTION("Masked diffBits with partial mask")
    {
        constexpr uint32_t PartialMask[3] = {0x0, 0xFFFFFFFF, 0x0};
        uint8_t count = diffBits<3>(Data, Ref, PartialMask);
        // Only middle word counts → 0x12345678 ^ 0x12345679 = 0x1 → 1 bit
        REQUIRE(count == 1);
    }

    SECTION("All identical arrays")
    {
        constexpr uint32_t Same[3] = {0xDEADBEEF, 0xCAFEBABE, 0x12345678};
        uint8_t count = diffBits<3>(Same, Same);
        REQUIRE(count == 0);
    }

    SECTION("All differences")
    {
        constexpr uint32_t AllDiff[3] = {0x0, 0x0, 0x0};
        uint8_t count = diffBits<3>(AllDiff, Ref);
        // Count total 1 bits in Ref[3]:
        // 0xAAAAAAAA → 16 bits
        // 0x12345678 → 13 bits
        // 0xFFFFFFFF → 32 bits
        // Total = 16+13+32 = 61
        REQUIRE(count == 61);
    }
}