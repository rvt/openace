
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#define private public

// #include <stdio.h>

// #include "pico/rand.h"
// #include "pico/time.h"
// #include "geomock.hpp"
#include "mockconfig.h"

// #include "flarm_utils.hpp"
#include "flarm2024.hpp"
// #include "ace/ognconv.hpp"
#include "mockutils.h"

class Test : public etl::message_router<Test, GATAS::AircraftPositionMsg>
{

public:
    GATAS::AircraftPositionInfo position;
    etl::imessage_bus *bus;
    Test(etl::imessage_bus *bus_) : bus(bus_)
    {
        bus->subscribe(*this);
    }
    ~Test()
    {
        bus->unsubscribe(*this);
    }

    void on_receive(const GATAS::AircraftPositionMsg &msg)
    {
        printf("AircraftPosition Received");
        position = msg.position;
    }
    void on_receive_unknown(const etl::imessage &msg)
    {
        (void)msg;
    }
};

GATAS::ThreadSafeBus<50> bus;
MockConfig mockConfig{bus};
Flarm2024 flarm{bus, mockConfig};
auto protocol = Radio::ProtocolConfig{1, GATAS::Modulation::GFSK, GATAS::DataSource::FLARM, 26, 8, 7, {0x99, 0xA5, 0xA9, 0x55, 0x66, 0x65, 0x96}}; // 0 FLARM 0 airtime 6ms

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

    for (int i = 3; i < 0xFF; i++)
    {
        REQUIRE(flarm.addressTypeFromFlarm(i) == GATAS::AddressType::RANDOM);
    }
}

