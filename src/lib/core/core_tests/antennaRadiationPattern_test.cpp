#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "pico/rand.h"
#include "pico/time.h"
#include "antennaRadiationPattern.hpp"

namespace
{
    constexpr float OWN_LAT = 52.0f;
    constexpr float OWN_LON = 4.0f;

    float lonOffsetDegrees(float eastMeters)
    {
        float cosLat = cosf(OWN_LAT * DEG_TO_RADS);
        if (fabsf(cosLat) < 0.01f)
        {
            cosLat = cosLat >= 0.0f ? 0.01f : -0.01f;
        }
        return eastMeters / (111321.0f * cosLat);
    }

    GATAS::IngressAircraftPositionMsg makeMsg(uint32_t distanceFromOwn, int32_t relNorthFromOwn, int32_t relEastFromOwn, int16_t track, int16_t rssiDbm)
    {
        GATAS::AircraftPositionInfo position{};
        position.distanceFromOwn = distanceFromOwn;
        position.lat = OWN_LAT + static_cast<float>(relNorthFromOwn) / 111139.0f;
        position.lon = OWN_LON + lonOffsetDegrees(static_cast<float>(relEastFromOwn));
        position.track = track;
        return GATAS::IngressAircraftPositionMsg(position, rssiDbm);
    }
}

TEST_CASE("put stores measurements in the expected radial", "[antennaRadiationPattern]")
{
    GATAS::AntennaRadiationPattern<8> pattern;

    pattern.put(makeMsg(15620, -10000, -12000, 45, -70), OWN_LAT, OWN_LON, 45.f);
    pattern.put(makeMsg(200, 100, 0, 90, -60), OWN_LAT, OWN_LON, 90.f);

    const auto &snapshot = pattern._radiationPattern();

    REQUIRE(snapshot[4].avgDistance == 7810);
    REQUIRE(snapshot[4].maxDistance == 15620);
    REQUIRE(snapshot[4].avgRssiDbm == -99);
    REQUIRE(snapshot[4].maxRssiDbm == -70);

    REQUIRE(snapshot[6].avgDistance == 100);
    REQUIRE(snapshot[6].maxDistance == 200);
    REQUIRE(snapshot[6].avgRssiDbm == -94);
    REQUIRE(snapshot[6].maxRssiDbm == -60);

    // untouched
    REQUIRE(snapshot[0].avgDistance == 0);
    REQUIRE(snapshot[0].maxDistance == 0);
    REQUIRE(snapshot[0].avgRssiDbm == -128);
    REQUIRE(snapshot[0].maxRssiDbm == -128);
}

TEST_CASE("calculateRelativeBearing uses ownship track and target position", "[antennaRadiationPattern]")
{
    using Pattern = GATAS::AntennaRadiationPattern<8>;

    REQUIRE(Pattern::calculateRelativeBearing(0, 100, 270.f) == Catch::Approx(180.f));
    REQUIRE(Pattern::calculateRelativeBearing(0, 100, 90.f) == Catch::Approx(0.f));
    REQUIRE(Pattern::calculateRelativeBearing(100, 0, 90.f) == Catch::Approx(270.f));
    REQUIRE(Pattern::calculateRelativeBearing(-100, 0, 90.f) == Catch::Approx(90.f));
}

TEST_CASE("put ignores target track when storing measurements", "[antennaRadiationPattern]")
{
    GATAS::AntennaRadiationPattern<8> pattern;

    pattern.put(makeMsg(200, 0, 100, 0, -60), OWN_LAT, OWN_LON, 270.f);

    const auto &snapshot = pattern._radiationPattern();

    REQUIRE(snapshot[4].avgDistance == 100);
    REQUIRE(snapshot[4].maxDistance == 200);
    REQUIRE(snapshot[4].avgRssiDbm == -94);
    REQUIRE(snapshot[4].maxRssiDbm == -60);

    REQUIRE(snapshot[0].avgDistance == 0);
    REQUIRE(snapshot[0].maxDistance == 0);
    REQUIRE(snapshot[0].avgRssiDbm == -128);
    REQUIRE(snapshot[0].maxRssiDbm == -128);
}
