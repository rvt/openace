
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "../include/adsl/utils.hpp"

using namespace ADSL;

// Q16 helper
constexpr float Q16_TO_FLOAT(int32_t q16) { return float(q16) / 32768.0f; }

TEST_CASE("distanceFast", "[single-file]")
{
  SECTION("Basic cases")
  {
    using namespace Catch::literals;
    REQUIRE(distanceFast(54, 4, 54, 4) == 0);

    REQUIRE(distanceFast(0, 4, 0, 5) == 111321_a);
    REQUIRE(distanceFast(1, 4, 0, 4) == 111139_a);

    Catch::Approx target = Catch::Approx(23370).margin(24);
    for (int lat = -50; lat <= 50; lat = lat + 100)
    {
      for (int lon = -4; lon <= 4; lon = lon + 8)
      {
        REQUIRE(distanceFast(lat + 0.1, lon + .6, lat + .3, lon + .7) ==
                target);
      }
    }
  }

  SECTION("Crossing datelines")
  {
    float lat = 0.0f;
    float lon1 = 179.999f;
    float lon2 = -179.999f;

    // ~224 meters apart
    REQUIRE(distanceFast(lat, lon1, lat, lon2) == Catch::Approx(224).margin(1));
  }
}

TEST_CASE("velocityFromTrack produces correct north/east components",
          "[navigation]")
{
  constexpr float eps = 1e-5f;

  SECTION("Track 0 deg (north)")
  {
    auto v = velocityFromTrack(45.0f, .0f);
    REQUIRE(v.north == Catch::Approx(0.0f).margin(eps));
    REQUIRE(v.east == Catch::Approx(0.0f).margin(eps));
  }

  SECTION("Track 0 deg (north)")
  {
    auto v = velocityFromTrack(0.0f, 10.0f);
    REQUIRE(v.north == Catch::Approx(10.0f).margin(eps));
    REQUIRE(v.east == Catch::Approx(0.0f).margin(eps));
  }

  SECTION("Track 90 deg (east)")
  {
    auto v = velocityFromTrack(90.0f, 10.0f);
    REQUIRE(v.north == Catch::Approx(0.0f).margin(eps));
    REQUIRE(v.east == Catch::Approx(10.0f).margin(eps));
  }

  SECTION("sweep of angles and a few speeds")
  {
    constexpr float eps = 1e-4f;
    for (float speed : {0.0f, 1.0f, 5.0f, 123.4f})
    {
      for (float track = -720.0f; track <= 720.0f; track += 0.5f)
      {
        auto v = velocityFromTrack(track, speed);

        float mag = std::sqrt(v.north * v.north + v.east * v.east);

        REQUIRE(mag == Catch::Approx(speed).margin(eps));
      }
    }
  }
}

TEST_CASE("northEastDistance", "[single-file]")
{
  using namespace Catch::literals;
  float lat, lon;

  lat = 50.0;
  lon = 4.0;
  auto result = northEastDistance(lat, lon, lat + 0.1, lon + 0.1);
  REQUIRE(result.east == 7155.56982_a);
  REQUIRE(result.north == 11113.73047_a);

  result = northEastDistance(lat, lon, lat - 0.1, lon - 0.1);
  REQUIRE(result.east == -7155.5698_a);
  REQUIRE(result.north == -11113.73047_a);
}

TEST_CASE("computeTCPA table driven cases", "[tcpa][param]")
{
  // clang-format off
  auto [r, ownTrack, ownSpeed, tgtTrack, tgtSpeed, expectedTcpa] =
      GENERATE(table<relNorthRelEast, float, float, float, float, float>({
          // r         ownTr ownSp tgtTr tgtSp   tcpa
          {{10000, 0},     0,  100,  180,  100,  50.0f},
          {{0, 1000},      0,  100,  270,  100,   5.0f},
          {{10000, 0},     0,  200,  180,  200,  25.0f},
          {{0, 1000},      0,  200,  270,  200,   2.5f},
          {{1000, 1000},  45,   50,   45,  50,   INFINITY},
          {{1000, 1000},  20,   50,   30,  50,   INFINITY},
          {{1000, -1000}, 20,   50,   30,  50,   152.477f},
      }));
  // clang-format on

  float tcpa = computeTCPA(r, ownTrack, ownSpeed, tgtTrack, tgtSpeed);

  if (std::isinf(expectedTcpa))
    REQUIRE(std::isinf(tcpa));
  else
    REQUIRE(tcpa == Catch::Approx(expectedTcpa).margin(1e-3f));
}

TEST_CASE("XXTEA_Encrypt_Key0 and XXTEA_Decrypt_Key0", "[single-file]")
{
  // Use std::array so we can compare before/after easily
  std::array<uint32_t, 8> original = {0x01, 0x02, 0x04, 0x08, 0x16, 0x32, 0x64, 0xF0};
  std::array<uint32_t, 8> buffer = original;

  // Encrypt in-place and ensure data changes
  XXTEA_Encrypt_Key0(buffer.data(), static_cast<uint8_t>(buffer.size()), static_cast<uint8_t>(4));
  REQUIRE(buffer != original);

  // Decrypt back and verify equality with original
  XXTEA_Decrypt_Key0(buffer.data(), static_cast<uint8_t>(buffer.size()), static_cast<uint8_t>(4));
  REQUIRE(buffer == original);
}
