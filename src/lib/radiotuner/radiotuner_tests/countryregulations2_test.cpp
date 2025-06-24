
#include <catch2/catch_test_macros.hpp>

#include "../ace/countryregulations2.hpp"
#include "pico/rand.h"
#include "pico/time.h"

auto countryRegulations = CountryRegulations2{};

TEST_CASE("zone", "[single-file]")
{
    REQUIRE(CountryRegulations2::ZONE1 == countryRegulations.zone(51, 4));
    REQUIRE(CountryRegulations2::ZONE4 == countryRegulations.zone(51, 120));
    REQUIRE(CountryRegulations2::ZONE1 == countryRegulations.zone(0, 0));
}

TEST_CASE("getFrequency", "[single-file]")
{
    REQUIRE(868200000 == CountryRegulations2::getFrequency(CountryRegulations2::Europe, CountryRegulations2::Channel::CH0));
    REQUIRE(868400000 == CountryRegulations2::getFrequency(CountryRegulations2::Europe, CountryRegulations2::Channel::CH1));
    REQUIRE(917000000 == CountryRegulations2::getFrequency(CountryRegulations2::Australia, CountryRegulations2::Channel::CH0));
    REQUIRE(917400000 == CountryRegulations2::getFrequency(CountryRegulations2::Australia, CountryRegulations2::Channel::CH1));
    REQUIRE(868200000 == CountryRegulations2::getFrequency(CountryRegulations2::Europe, CountryRegulations2::Channel::NOP));
}

TEST_CASE("getSlot", "[single-file]")
{
    REQUIRE(1 == CountryRegulations2::getSlot(CountryRegulations2::ZONE6, GATAS::DataSource::PAW).ptsId);
    REQUIRE(4 == CountryRegulations2::getSlot(CountryRegulations2::ZONE1, GATAS::DataSource::ADSL).ptsId);
    REQUIRE(2 == CountryRegulations2::getSlot(CountryRegulations2::ZONE1, GATAS::DataSource::FLARM).ptsId);
}
