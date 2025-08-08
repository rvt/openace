
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "ace/messagerouter.hpp"
#include "ace/messages.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>

#define private public

const uint8_t DEFAULT_GATAS_CONFIG[] = R"=(
{"signature": 250207}
)=";

#include "inmemorystore.hpp"
#include "config.hpp"

GATAS::ThreadSafeBus<50> bus;

TEST_CASE( "Fully Configured", "[single-file]" )
{
    InMemoryStore store;
    InMemoryStore permanentStore;
    //std::filesystem::path currentDir = std::filesystem::current_path();
    //std::cout << "Current execution directory: " << currentDir << std::endl;

    std::ifstream inputFile("./test.json"); // Open the file
    if (!inputFile)
    {
        std::cerr << "Error opening file." << std::endl;
        REQUIRE_FALSE(true);
    }
    inputFile.seekg(0, std::ios::end);
    std::streampos fileSize = inputFile.tellg();
    std::vector<char> buffer(fileSize);

    // Determine the size of the file
    inputFile.seekg(0, std::ios::beg);
    inputFile.read(buffer.data(), fileSize);
    inputFile.close();
    permanentStore.write((uint8_t*)buffer.data(), fileSize );

    Config config(bus, store, permanentStore, DEFAULT_GATAS_CONFIG);
    config.postConstruct();

    SECTION( "IPv4", "[single-file]" )
    {
        REQUIRE( 1689430208 == config.parseIpv4String("192.168.178.100", 0xffffffffUL));
        REQUIRE( 0xffffffff == config.parseIpv4String("300", 0xffffffffUL));
        REQUIRE( 43200 == config.parseIpv4String("192.168", 0xffffffffUL));
    }

    SECTION( "AircraftHwConfig" )
    {
        auto hwConfig = config.gaTasConfig();
        REQUIRE( 12345678 == hwConfig.conspicuity.address );
        REQUIRE( GATAS::AddressType::OGN == hwConfig.conspicuity.addressType );
        REQUIRE( GATAS::AircraftCategory::SMALL == hwConfig.conspicuity.category );
//        REQUIRE ( (hwConfig.privacy == 0) );
        REQUIRE( 3 == hwConfig.protocols.size() );
        REQUIRE( GATAS::DataSource::OGN1 == hwConfig.protocols[0] );
        REQUIRE( GATAS::DataSource::ADSL == hwConfig.protocols[1] );
        REQUIRE( GATAS::DataSource::FLARM == hwConfig.protocols[2] );
    }

    SECTION( "Arrays" )
    {
        // char path[] = "defaultPorts/X";
        // path[sizeof(path) - 2] = i;
        // int32_t port = config.valueByPath(GDL90OVERUDP_DEFAULT_PORT, NAME, path);

        REQUIRE( 4000 == config.valueByPath(1, "GDLoverUDP/defaultPorts/0", "") );
        REQUIRE( 4001 == config.valueByPath(1, "GDLoverUDP/defaultPorts/1", "") );
        auto value = config.strValueByPath("-", "GDLoverUDP/ips/0", "ip");
        etl::string<24> expect = "192.168.178.192";
        REQUIRE( expect == value );
    }

    SECTION( "pinMap" )
    {
        SECTION( "valid" )
        {
            GATAS::PinTypeMap map = config.pinMap("Sx1262_1");
            REQUIRE( map.size() == 4 );
            REQUIRE( map[GATAS::PinType::BUSY] == 13 );
            REQUIRE( map[GATAS::PinType::CS] == 12 );
            REQUIRE( map[GATAS::PinType::DIO1] == 19 );
            REQUIRE( map[GATAS::PinType::SPI] == 0 );
        }

        SECTION( "NoPort andInvalidPort" )
        {
            GATAS::PinTypeMap map = config.pinMap("NoPort");
            REQUIRE( (map.size() == 0) );
            map = config.pinMap("InvalidPort");
            REQUIRE( (map.size() == 0) );
        }

    }
}

TEST_CASE( "Config GATAS::PostConstruct", "[single-file]" )
{
    InMemoryStore store;
    InMemoryStore permanentStore;

    const uint8_t WRONG[] = "INIT WRONG";
    store.write(WRONG, sizeof(WRONG));

    SECTION( "Default if stores are wrong") {
        Config testCfg{bus, store, permanentStore, DEFAULT_GATAS_CONFIG};
        REQUIRE( testCfg.postConstruct() == GATAS::PostConstruct::OK );
        REQUIRE( 250207 == testCfg.valueByPath(1, "signature", "") );
    }

}
