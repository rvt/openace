
#include <catch2/catch_test_macros.hpp>

#include "../ace/countryregulations_v2.hpp"
#include "pico/rand.h"
#include "pico/time.h"

auto countryRegulations = CountryRegulations{};

TEST_CASE("zone", "[single-file]")
{
    REQUIRE(CountryRegulations::ZONE1 == countryRegulations.zone(51, 4));
    REQUIRE(CountryRegulations::ZONE4 == countryRegulations.zone(51, 120));
    REQUIRE(CountryRegulations::ZONE1 == countryRegulations.zone(0, 0));
}

TEST_CASE("getFrequency", "[single-file]")
{
    REQUIRE(868200000 == CountryRegulations::getFrequency(CountryRegulations::Europe, CountryRegulations::Channel::CH00));
    REQUIRE(868400000 == CountryRegulations::getFrequency(CountryRegulations::Europe, CountryRegulations::Channel::CH01));
    REQUIRE(917000000 == CountryRegulations::getFrequency(CountryRegulations::Australia, CountryRegulations::Channel::CH00));
    REQUIRE(917400000 == CountryRegulations::getFrequency(CountryRegulations::Australia, CountryRegulations::Channel::CH01));
    REQUIRE(868200000 == CountryRegulations::getFrequency(CountryRegulations::Europe, CountryRegulations::Channel::NOOP));
}

TEST_CASE("getSlot", "[single-file]")
{
    REQUIRE(1 == CountryRegulations::getSlot(CountryRegulations::ZONE6, GATAS::DataSource::PAW).ptsId);
    REQUIRE(4 == CountryRegulations::getSlot(CountryRegulations::ZONE1, GATAS::DataSource::ADSL).ptsId);
    REQUIRE(2 == CountryRegulations::getSlot(CountryRegulations::ZONE1, GATAS::DataSource::FLARM).ptsId);
}

TEST_CASE("getFrequency lat/lon/datasource", "[single-file]")
{
    auto frequency =  CountryRegulations::getFrequency(37.420, -121.714, GATAS::DataSource::FLARM);

    REQUIRE(frequency == 910200000);
}
