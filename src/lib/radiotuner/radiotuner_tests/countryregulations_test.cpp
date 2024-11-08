
#include <catch2/catch_test_macros.hpp>

#include "../ace/countryregulations.hpp"
#include "pico/rand.h"
#include "pico/time.h"

auto countryRegulations = CountryRegulations{};

TEST_CASE("zone", "[single-file]")
{
    REQUIRE(CountryRegulations::ZONE1 == countryRegulations.zone(51, 4));
    REQUIRE(CountryRegulations::ZONE4 == countryRegulations.zone(51, 120));
    REQUIRE(CountryRegulations::ZONE1 == countryRegulations.zone(0, 0));
}

TEST_CASE("nextProtocolTimeslot", "[single-file]")
{
    uint8_t idx;
    idx = countryRegulations.nextProtocolTimeslot(100, CountryRegulations::Zone::ZONE1, OpenAce::DataSource::FLARM);
    REQUIRE(400 == countryRegulations.protocolTimeslotById(idx).slotStartTime);

    idx = countryRegulations.nextProtocolTimeslot(500, CountryRegulations::Zone::ZONE1, OpenAce::DataSource::FLARM);
    REQUIRE(800 == countryRegulations.protocolTimeslotById(idx).slotStartTime);

    idx = countryRegulations.nextProtocolTimeslot(900, CountryRegulations::Zone::ZONE1, OpenAce::DataSource::FLARM);
    REQUIRE(400 == countryRegulations.protocolTimeslotById(idx).slotStartTime);

    idx = countryRegulations.nextProtocolTimeslot(300, CountryRegulations::Zone::ZONE1, OpenAce::DataSource::FLARM);
    REQUIRE(400 == countryRegulations.protocolTimeslotById(idx).slotStartTime);

    idx = countryRegulations.nextProtocolTimeslot(1000, CountryRegulations::Zone::ZONE1, OpenAce::DataSource::FLARM);
    REQUIRE(400 == countryRegulations.protocolTimeslotById(idx).slotStartTime);

    idx = countryRegulations.nextProtocolTimeslot(0, CountryRegulations::Zone::ZONE1, OpenAce::DataSource::FLARM);
    REQUIRE(400 == countryRegulations.protocolTimeslotById(idx).slotStartTime);

    // Unconfigured ZONE/Datasource
    idx = countryRegulations.nextProtocolTimeslot(0, CountryRegulations::Zone::ZONE2, OpenAce::DataSource::FLARM);
    REQUIRE(CountryRegulations::Zone::ZONE0 == countryRegulations.protocolTimeslotById(idx).zone);
}

TEST_CASE("findFittingTimeslot", "[single-file]")
{
    uint8_t idx;
    idx = countryRegulations.nextProtocolTimeslot(500, CountryRegulations::Zone::ZONE1, OpenAce::DataSource::FLARM);
    REQUIRE(2 == countryRegulations.findFittingTimeslot(100, idx));
    REQUIRE(2 == countryRegulations.findFittingTimeslot(1100, idx));
    REQUIRE(0 == countryRegulations.findFittingTimeslot(1300, idx));
    REQUIRE(0 == countryRegulations.findFittingTimeslot(300, idx));
    REQUIRE(1 == countryRegulations.findFittingTimeslot(500, idx));
    REQUIRE(1 == countryRegulations.findFittingTimeslot(1500, idx));
    REQUIRE(2 == countryRegulations.findFittingTimeslot(900, idx));
    REQUIRE(2 == countryRegulations.findFittingTimeslot(1900, idx));
}

TEST_CASE("protocolTimeslotById", "[single-file]")
{
    uint8_t idx;

    idx = countryRegulations.getFirstSlotIdx(CountryRegulations::Zone::ZONE1, OpenAce::DataSource::FLARM);
    REQUIRE(1 == countryRegulations.protocolTimeslotById(idx).idx);
}

TEST_CASE("getFirstSlotIdx", "[single-file]")
{
    uint8_t idx;

    idx = countryRegulations.getFirstSlotIdx(CountryRegulations::Zone::ZONE1, OpenAce::DataSource::FLARM);
    REQUIRE(idx + 1 == countryRegulations.nextSlotIdx(CountryRegulations::Zone::ZONE1, idx));
    // FLARM has two slots for ZONE1, so should warm over
    REQUIRE(idx == countryRegulations.nextSlotIdx(CountryRegulations::Zone::ZONE1, idx + 1));
}

TEST_CASE("getNextTxTime", "[single-file]")
{
    // When within time
    get_rand_64_SET(200 << 4);
    auto delay = CountryRegulations::getNextTxTime(100, 1);
    REQUIRE(delay.duration == 800); // 200 + 600
    REQUIRE(delay.idx == 2);

    // When outside time failSafe == MAXFAILSAFE and start at 400ms
    get_rand_64_SET(300 << 4);
    delay = CountryRegulations::getNextTxTime(400, 1);
    REQUIRE(delay.duration == 600); // 600 (txMinTime)
    REQUIRE(delay.idx == 2);

    // larger time
    get_rand_64_SET(790 << 4);
    delay = CountryRegulations::getNextTxTime(400, 2);
    REQUIRE(delay.duration == 1390); // 790 + 600
    REQUIRE(delay.idx == 1);

    // Flips over whole second for FLARM
    get_rand_64_SET(280 << 4);
    delay = CountryRegulations::getNextTxTime(100, 2);
    REQUIRE(delay.duration == 905); // 280 + 600 + 25
    REQUIRE(delay.idx == 2);

    // Not Flips over whole second for FLARM
    get_rand_64_SET(280 << 4);
    delay = CountryRegulations::getNextTxTime(100, 7);
    REQUIRE(delay.duration == 880); // 280 + 600 + 25
    REQUIRE(delay.idx == 8);
}