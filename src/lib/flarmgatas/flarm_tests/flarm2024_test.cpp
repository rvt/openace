
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#define private public

#include "mockconfig.h"

#include "flarm2024.hpp"
#include "ace/ognconv.hpp"
#include "ace/messagerouter.hpp"
#include "testhelpers.h"

GATAS::ThreadSafeBus<50> bus;
MockConfig mockConfig{bus};
Flarm2024 flarm{bus, mockConfig};

TEST_CASE("addressTypeToFlarm", "[single-file]")
{
    REQUIRE(flarm.addressTypeToFlarm(GATAS::AddressType::RANDOM) == 0);
    REQUIRE(flarm.addressTypeToFlarm(GATAS::AddressType::ICAO) == 1);
    REQUIRE(flarm.addressTypeToFlarm(GATAS::AddressType::FLARM) == 2);
    REQUIRE(flarm.addressTypeToFlarm(GATAS::AddressType::FANET) == 0);
    REQUIRE(flarm.addressTypeToFlarm(GATAS::AddressType::OGN) == 0);
}

TEST_CASE("addressTypeFromFlarm", "[single-file]")
{
    REQUIRE(flarm.addressTypeFromFlarm(0) == GATAS::AddressType::RANDOM);
    REQUIRE(flarm.addressTypeFromFlarm(1) == GATAS::AddressType::ICAO);
    REQUIRE(flarm.addressTypeFromFlarm(2) == GATAS::AddressType::FLARM);

    REQUIRE(flarm.addressTypeFromFlarm(3) == GATAS::AddressType::RANDOM);
    REQUIRE(flarm.addressTypeFromFlarm(255) == GATAS::AddressType::RANDOM);
}


// ─────────────────────────────────────────────
// toAircraftCategory
// ─────────────────────────────────────────────
TEST_CASE("toAircraftCategory - explicit mappings", "[Flarm2024][aircraftCategory]")
{
    CHECK(flarm.toAircraftCategory(1)    == GATAS::AircraftCategory::GLIDER);
    CHECK(flarm.toAircraftCategory(2)    == GATAS::AircraftCategory::LIGHT);
    CHECK(flarm.toAircraftCategory(3)    == GATAS::AircraftCategory::ROTORCRAFT);
    CHECK(flarm.toAircraftCategory(4)    == GATAS::AircraftCategory::SKY_DIVER);
    CHECK(flarm.toAircraftCategory(5)    == GATAS::AircraftCategory::DROP_PLANE);
    CHECK(flarm.toAircraftCategory(6)    == GATAS::AircraftCategory::HANG_GLIDER);
    CHECK(flarm.toAircraftCategory(7)    == GATAS::AircraftCategory::PARA_GLIDER);
    CHECK(flarm.toAircraftCategory(8)    == GATAS::AircraftCategory::LIGHT);       // shares case with 2
    CHECK(flarm.toAircraftCategory(9)    == GATAS::AircraftCategory::LARGE);
    CHECK(flarm.toAircraftCategory(0x0b) == GATAS::AircraftCategory::LIGHT_THAN_AIR);
    CHECK(flarm.toAircraftCategory(0x0c) == GATAS::AircraftCategory::LIGHT_THAN_AIR); // shares case with 0x0b
    CHECK(flarm.toAircraftCategory(0x0d) == GATAS::AircraftCategory::UN_MANNED);
    CHECK(flarm.toAircraftCategory(0x0f) == GATAS::AircraftCategory::POINT_OBSTACLE);
}

TEST_CASE("toAircraftCategory - unmapped codes return UNKNOWN", "[Flarm2024][aircraftCategory]")
{
    CHECK(flarm.toAircraftCategory(0)    == GATAS::AircraftCategory::UNKNOWN);
    CHECK(flarm.toAircraftCategory(0x0a) == GATAS::AircraftCategory::UNKNOWN); // gap between 9 and 0x0b
    CHECK(flarm.toAircraftCategory(0x0e) == GATAS::AircraftCategory::UNKNOWN); // gap between 0x0d and 0x0f
    CHECK(flarm.toAircraftCategory(0xFF) == GATAS::AircraftCategory::UNKNOWN);
}

// ─────────────────────────────────────────────
// fromAircraftCategory
// ─────────────────────────────────────────────
TEST_CASE("fromAircraftCategory - explicit mappings", "[Flarm2024][aircraftCategory]")
{
    CHECK(flarm.fromAircraftCategory(GATAS::AircraftCategory::GLIDER)          == 0x01);
    CHECK(flarm.fromAircraftCategory(GATAS::AircraftCategory::ROTORCRAFT)      == 0x03);
    CHECK(flarm.fromAircraftCategory(GATAS::AircraftCategory::GYROCOPTER)      == 0x03); // shares case
    CHECK(flarm.fromAircraftCategory(GATAS::AircraftCategory::SKY_DIVER)       == 0x04);
    CHECK(flarm.fromAircraftCategory(GATAS::AircraftCategory::DROP_PLANE)      == 0x05);
    CHECK(flarm.fromAircraftCategory(GATAS::AircraftCategory::HANG_GLIDER)     == 0x06);
    CHECK(flarm.fromAircraftCategory(GATAS::AircraftCategory::PARA_GLIDER)     == 0x07);
    CHECK(flarm.fromAircraftCategory(GATAS::AircraftCategory::LIGHT)           == 0x08);
    CHECK(flarm.fromAircraftCategory(GATAS::AircraftCategory::ULTRA_LIGHT_FIXED_WING) == 0x08); // shares case
    CHECK(flarm.fromAircraftCategory(GATAS::AircraftCategory::SMALL)           == 0x08);
    CHECK(flarm.fromAircraftCategory(GATAS::AircraftCategory::HIGH_VORTEX)     == 0x09);
    CHECK(flarm.fromAircraftCategory(GATAS::AircraftCategory::HEAVY_ICAO)      == 0x09);
    CHECK(flarm.fromAircraftCategory(GATAS::AircraftCategory::AEROBATIC)       == 0x09);
    CHECK(flarm.fromAircraftCategory(GATAS::AircraftCategory::LARGE)           == 0x09);
    CHECK(flarm.fromAircraftCategory(GATAS::AircraftCategory::LIGHT_THAN_AIR)  == 0x0b);
    CHECK(flarm.fromAircraftCategory(GATAS::AircraftCategory::UN_MANNED)       == 0x0d);
    CHECK(flarm.fromAircraftCategory(GATAS::AircraftCategory::POINT_OBSTACLE)  == 0x0f);
}

TEST_CASE("fromAircraftCategory - no direct equivalent returns 0", "[Flarm2024][aircraftCategory]")
{
    CHECK(flarm.fromAircraftCategory(GATAS::AircraftCategory::UNKNOWN)                   == 0);
    CHECK(flarm.fromAircraftCategory(GATAS::AircraftCategory::MILITARY)                  == 0);
    CHECK(flarm.fromAircraftCategory(GATAS::AircraftCategory::SPACE_VEHICLE)             == 0);
    CHECK(flarm.fromAircraftCategory(GATAS::AircraftCategory::SURFACE_EMERGENCY_VEHICLE) == 0);
    CHECK(flarm.fromAircraftCategory(GATAS::AircraftCategory::SURFACE_VEHICLE)           == 0);
    CHECK(flarm.fromAircraftCategory(GATAS::AircraftCategory::CLUSTER_OBSTACLE)          == 0);
    CHECK(flarm.fromAircraftCategory(GATAS::AircraftCategory::LINE_OBSTACLE)             == 0);
}

// ─────────────────────────────────────────────
// Round-trip: aircraftCategory (only lossless mappings)
// ─────────────────────────────────────────────
TEST_CASE("aircraftCategory round-trip for lossless mappings", "[Flarm2024][aircraftCategory]")
{
    // These are 1:1 — from→to→from should return the same category
    using C = GATAS::AircraftCategory;
    for (auto cat : {C::GLIDER, C::SKY_DIVER, C::DROP_PLANE,
                     C::HANG_GLIDER, C::PARA_GLIDER,
                     C::LIGHT_THAN_AIR, C::UN_MANNED, C::POINT_OBSTACLE})
    {
        INFO("Category: " << static_cast<int>(cat));
        CHECK(flarm.toAircraftCategory(flarm.fromAircraftCategory(cat)) == cat);
    }
}