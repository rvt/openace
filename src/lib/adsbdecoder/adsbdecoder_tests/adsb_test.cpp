
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "math.h"
#include <fstream>
#include <filesystem>
#include <stdio.h>
#include "mockutils.h"
#include "mockconfig.h"
#include "semphr.h"
#include "ace/coreutils.hpp"

#define private public

#include "adsbdecoder.hpp"

class Test : public etl::message_router<Test, OpenAce::AircraftPositionMsg>
{

public:
    OpenAce::AircraftPositionInfo position;
    etl::imessage_bus *bus;
    bool received = false;
    Test(etl::imessage_bus *bus_) : bus(bus_)
    {
        bus->subscribe(*this);
    }
    ~Test()
    {
        bus->unsubscribe(*this);
    }

    void on_receive(const OpenAce::AircraftPositionMsg &msg)
    {
        // printf("AircraftPosition Received\n");
        received = true;
        position = msg.position;
    }
    void on_receive_unknown(const etl::imessage &msg)
    {
        (void)msg;
    }
};

OpenAce::ThreadSafeBus<50> bus;
MockConfig mockConfig{bus};

auto ownship = OpenAce::OwnshipPositionInfo{
    CoreUtils::timeUs32(),
    true,
    52.2,
    4.2,
    1500, // Altitude above WGS84 ellipsoid in meters
    0,    // in m/s
    50,   // in m/s
    0,    // 0..359
    0,    // deg/s Turn rate in the horizontal plane
    50,   // North velocity in m/s
    0,    // East velocity in m/s
    1500, // Height above egm96, eg MSL
    1500  // Height of geoid above WGS84 ellipsoid
};

// This test needs a bit more validation
TEST_CASE("Test filter below and above", "[single-file]")
{
    xSemaphoreTakeValue = pdTRUE;
    ADSBDecoder adsbDecoder{bus, mockConfig};
    adsbDecoder.on_receive(OpenAce::AdapativeRadiusMsg{10'000'000});
    adsbDecoder.on_receive(OpenAce::OwnshipPositionMsg{ownship});
    adsbDecoder.postConstruct();
    uint8_t data[24];
    Test test{&bus};

    namespace fs = std::filesystem;
    std::string filename = "adsb.txt";
    auto path = fs::current_path();
    printf("->-> Path: %s\n", path.c_str());
    std::ifstream infile(filename);

    REQUIRE (infile.is_open());

    int higestPlane = 0;
    int lowestPlane = 50000;

    std::string line;
    adsbDecoder.ownshipPosition.altitudeWgs84 = 10000;
    adsbDecoder.filterAbove = 50000;
    adsbDecoder.filterBelow = 50000;
    while (std::getline(infile, line))
    {
        test.received = false;
        get_absolute_timeValue++;
        CoreUtils::hexStrToByteArray(line.c_str() + 1, data);
        adsbDecoder.receiveBinary(data, 14);

        if (test.received)
        {
            REQUIRE(test.position.address != 0);

            if (test.position.altitudeWgs84 > higestPlane)
            {
                higestPlane = test.position.altitudeWgs84;
            }
            if (test.position.altitudeWgs84 < lowestPlane)
            {
                lowestPlane = test.position.altitudeWgs84;
            }
        }
    }
    printf("Highest Plane: %dm lowestPlane: %dm\n", higestPlane, lowestPlane);

    int totalPlanes = 0;
    // Test Filtering planes above me
    infile.clear();                 // clear fail and eof bits
    infile.seekg(0, std::ios::beg); // back to the start!
    adsbDecoder.filterAbove = 1000;
    adsbDecoder.filterBelow = 1000;
    adsbDecoder.ownshipPosition.altitudeWgs84 = lowestPlane - adsbDecoder.filterAbove;
    get_absolute_timeValue += 10000000;
    while (std::getline(infile, line))
    {
        test.received = false;
        get_absolute_timeValue++;
        CoreUtils::hexStrToByteArray(line.c_str() + 1, data);
        adsbDecoder.receiveBinary(data, 14);
        if (test.received)
        {
            totalPlanes++;
        }
    }
    printf("Total Planes above: %d\n", totalPlanes);
    REQUIRE(totalPlanes == 4);

    // Test Filtering planes below me
    infile.clear();                 // clear fail and eof bits
    infile.seekg(0, std::ios::beg); // back to the start!
    adsbDecoder.postConstruct();    // Also clears current internal status
    adsbDecoder.filterAbove = 1000;
    adsbDecoder.filterBelow = 1000;
    adsbDecoder.ownshipPosition.altitudeWgs84 = higestPlane;
    get_absolute_timeValue += 100'000'000;
    totalPlanes = 0;
    while (std::getline(infile, line))
    {
        test.received = false;
        get_absolute_timeValue += 10000;
        time_us_64Value += 10000;
        CoreUtils::hexStrToByteArray(line.c_str() + 1, data);
        adsbDecoder.receiveBinary(data, line.size() - 1);
        if (test.received)
        {
            totalPlanes++;
        }
        adsbDecoder.on_receive(OpenAce::IdleMsg());
    }
    printf("Total Planes below: %d\n", totalPlanes);
    REQUIRE(totalPlanes == 379);
    infile.close();
}

TEST_CASE("Test heading and direction received aircraft", "[single-file]")
{
    xSemaphoreTakeValue = pdTRUE;
    ADSBDecoder adsbDecoder{bus, mockConfig};
    adsbDecoder.on_receive(OpenAce::AdapativeRadiusMsg{1'000'000});
    adsbDecoder.on_receive(OpenAce::OwnshipPositionMsg{ownship});
    adsbDecoder.postConstruct();
    Test test{&bus};
    adsbDecoder.filterAbove = 50000;
    adsbDecoder.filterBelow = 50000;

    test.received = false;
    adsbDecoder.ownshipPosition.altitudeWgs84 = 10000;
    adsbDecoder.ownshipPosition.lat = 52.1;
    adsbDecoder.ownshipPosition.lon = 4.8;

    uint8_t data[14];
    CoreUtils::hexStrToByteArray("8d502cd1589992ecbaf1a4140b65", data);
    adsbDecoder.receiveBinary(data, 14);
    CoreUtils::hexStrToByteArray("8d502cd15899965802eb001f31d8", data);
    adsbDecoder.receiveBinary(data, 14);
    CoreUtils::hexStrToByteArray("8d502cd19908c532903c9cced691", data);
    adsbDecoder.receiveBinary(data, 14);

    REQUIRE(test.received == true);
    REQUIRE(test.position.address == 0x502CD1);
    REQUIRE(test.position.addressType == OpenAce::AddressType::ICAO);
    REQUIRE(test.position.dataSource == OpenAce::DataSource::ADSB);
    REQUIRE(test.position.aircraftType == OpenAce::AircraftCategory::Unknown);
    REQUIRE(test.position.altitudeWgs84 == 9029);
    REQUIRE(test.position.groundSpeed == Catch::Approx(230.98).margin(0.1)); // in m/s
    REQUIRE(test.position.course == 25);                                     // ADSB data shows 25.94.. Should we use floats instead of int?
    REQUIRE(test.position.lat == Catch::Approx(52.3888).margin(0.005));
    REQUIRE(test.position.lon == Catch::Approx(4.7209).margin(0.005));
    REQUIRE(test.position.verticalSpeed == Catch::Approx(4.552).margin(0.001));

    REQUIRE(test.position.distanceFromOwn == Catch::Approx(32551).margin(1));
    REQUIRE(test.position.relNorthFromOwn == Catch::Approx(32107).margin(1));
    REQUIRE(test.position.relEastFromOwn == Catch::Approx(-5359).margin(1));
    REQUIRE(test.position.bearingFromOwn == Catch::Approx(351).margin(0.5));
}

TEST_CASE("Test descending aircraft", "[single-file]")
{
    xSemaphoreTakeValue = pdTRUE;
    ADSBDecoder adsbDecoder{bus, mockConfig};
    adsbDecoder.on_receive(OpenAce::AdapativeRadiusMsg{1'000'000});
    adsbDecoder.on_receive(OpenAce::OwnshipPositionMsg{ownship});
    adsbDecoder.postConstruct();
    Test test{&bus};
    adsbDecoder.filterAbove = 50000;
    adsbDecoder.filterBelow = 50000;

    test.received = false;
    adsbDecoder.ownshipPosition.altitudeWgs84 = 10000;
    adsbDecoder.ownshipPosition.lat = 52.1;
    adsbDecoder.ownshipPosition.lon = 4.8;

    uint8_t data[14];
    CoreUtils::hexStrToByteArray("8d407a055817867d1ce5ecbe8fdd", data); // odd
    adsbDecoder.receiveBinary(data, 14);
    CoreUtils::hexStrToByteArray("8d407a0558178312d6eefaca54eb", data); // event
    adsbDecoder.receiveBinary(data, 14);
    CoreUtils::hexStrToByteArray("8d407a059908e102980c93715499", data);
    adsbDecoder.receiveBinary(data, 14);

    REQUIRE(test.position.verticalSpeed == Catch::Approx(-0.65024).margin(0.001));
}
