#include <catch2/catch_test_macros.hpp>

#include <string_view>

#include "models.hpp"

TEST_CASE("DataSource short strings stay compact and stable", "[models]")
{
    REQUIRE(std::string_view(GATAS::toShortString(GATAS::DataSource::FLARM)) == "FL");
    REQUIRE(std::string_view(GATAS::toShortString(GATAS::DataSource::ADSLM)) == "AD");
    REQUIRE(std::string_view(GATAS::toShortString(GATAS::DataSource::ADSLO_HDR)) == "AH");
    REQUIRE(std::string_view(GATAS::toShortString(GATAS::DataSource::ADSLFLARM)) == "AF");
    REQUIRE(std::string_view(GATAS::toShortString(GATAS::DataSource::ADSLOGN)) == "AO");
    REQUIRE(std::string_view(GATAS::toShortString(GATAS::DataSource::FANET)) == "FA");
    REQUIRE(std::string_view(GATAS::toShortString(GATAS::DataSource::ADSB)) == "AB");
    REQUIRE(std::string_view(GATAS::toShortString(GATAS::DataSource::OGN)) == "OG");
    REQUIRE(std::string_view(GATAS::toShortString(GATAS::DataSource::NONE)) == "NO");
}
