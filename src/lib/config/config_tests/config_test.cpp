
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "ace/messagerouter.hpp"
#include "ace/messages.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>
#include "pico/rand.h"

#define private public

const uint8_t DEFAULT_GATAS_CONFIG[] = R"=(
{"signature": 250801}
)=";

#include "inmemorystore.hpp"
#include "config.hpp"

GATAS::ThreadSafeBus<50> bus;

TEST_CASE("Fully Configured", "[single-file]")
{
    uint8_t vstore[4096] = {};
    uint8_t pstore[4096] = {};
    uint8_t btore[4096] = {};

    InMemoryStore volatileStore{4096, vstore};
    InMemoryStore binaryStore{4096, btore};
    InMemoryStore permanentStore{4096, pstore};
    // std::filesystem::path currentDir = std::filesystem::current_path();
    // std::cout << "Current execution directory: " << currentDir << std::endl;

    std::ifstream inputFile("./test.json"); // Open the file
    REQUIRE(inputFile.is_open());

    inputFile.seekg(0, std::ios::end);
    std::streampos fileSize = inputFile.tellg();
    std::vector<char> buffer(fileSize);

    // Determine the size of the file
    inputFile.seekg(0, std::ios::beg);
    inputFile.read(buffer.data(), fileSize);
    inputFile.close();
    REQUIRE(inputFile.gcount() == fileSize);

    permanentStore.write((uint8_t *)buffer.data(), fileSize);
    Config config(bus, volatileStore, permanentStore, binaryStore, DEFAULT_GATAS_CONFIG);
    config.postConstruct();

    SECTION("IPv4", "[single-file]")
    {
        REQUIRE(1689430208 == config.parseIpv4String("192.168.178.100", 0xffffffffUL));
        REQUIRE(0xffffffff == config.parseIpv4String("300", 0xffffffffUL));
        REQUIRE(43200 == config.parseIpv4String("192.168", 0xffffffffUL));
    }

    SECTION("AircraftHwConfig")
    {
        auto hwConfig = config.gaTasConfig();
        REQUIRE(static_cast<uint32_t>(12345678) == static_cast<uint32_t>(hwConfig.conspicuity.icaoAddress));
        REQUIRE(GATAS::AddressType::OGN == hwConfig.conspicuity.addressType);
        REQUIRE(GATAS::AircraftCategory::SMALL == hwConfig.conspicuity.category);
        //        REQUIRE ( (hwConfig.privacy == 0) );
        REQUIRE(3 == hwConfig.protocols.size());
        REQUIRE(GATAS::DataSource::OGN1 == hwConfig.protocols[0]);
        REQUIRE(GATAS::DataSource::ADSL == hwConfig.protocols[1]);
        REQUIRE(GATAS::DataSource::FLARM == hwConfig.protocols[2]);
    }

    SECTION("Arrays")
    {
        REQUIRE(4000 == config.valueByPath(1, "GDLoverUDP/defaultPorts/0", ""));
        REQUIRE(4001 == config.valueByPath(1, "GDLoverUDP/defaultPorts/1", ""));

        REQUIRE(4000 == config.valueByPath(1, "GDLoverUDP", "defaultPorts/0"));
        REQUIRE(4001 == config.valueByPath(1, "GDLoverUDP", "defaultPorts/1"));

        auto value = config.strValueByPath("-", "GDLoverUDP/ips/0", "ip");
        etl::string<24> expect = "192.168.178.192";
        REQUIRE(expect == value);
    }

    SECTION("pinMap")
    {
        SECTION("valid")
        {
            GATAS::PinTypeMap map = config.pinMap("Sx1262_1");
            REQUIRE(map.size() == 4);
            REQUIRE(map[GATAS::PinType::BUSY] == 13);
            REQUIRE(map[GATAS::PinType::CS] == 12);
            REQUIRE(map[GATAS::PinType::DIO1] == 19);
            REQUIRE(map[GATAS::PinType::SPI] == 0);
        }

        SECTION("NoPort andInvalidPort")
        {
            GATAS::PinTypeMap map = config.pinMap("NoPort");
            REQUIRE((map.size() == 0));
            map = config.pinMap("InvalidPort");
            REQUIRE((map.size() == 0));
        }
    }

    SECTION("value by path")
    {
        config.setValueBypath("/Bluetooth/localName", "bas");
        REQUIRE("bas" == config.strValueByPath("default", "Bluetooth", "localName"));
        config.setValueBypath("/Bluetooth/localName", "bar");
        REQUIRE("bar" == config.strValueByPath("default", "Bluetooth", "localName"));

        config.setValueBypath("/ADSBDecoder/filterAbove", 12);
        REQUIRE(12 == config.valueByPath(0, "ADSBDecoder", "filterAbove"));
        config.setValueBypath("/ADSBDecoder/filterAbove", 15);
        REQUIRE(15 == config.valueByPath(0, "ADSBDecoder", "filterAbove"));
    }
}

TEST_CASE("Config GATAS::PostConstruct", "[single-file]")
{
    uint8_t vstore[4096] = {};
    uint8_t pstore[4096] = {};
    uint8_t btore[4096] = {};

    InMemoryStore volatileStore{4096, vstore};
    InMemoryStore binaryStore{4096, btore};
    InMemoryStore permanentStore{4096, pstore};

    const uint8_t WRONG[] = "INIT WRONG";
    permanentStore.write(WRONG, sizeof(WRONG));

    SECTION("Default if stores are wrong")
    {
        get_rand_64_SET(1234);
        Config testCfg{bus, volatileStore, permanentStore, binaryStore, DEFAULT_GATAS_CONFIG};
        REQUIRE(testCfg.postConstruct() == GATAS::PostConstruct::OK);
        REQUIRE(250801 == testCfg.valueByPath(1, "signature", ""));
        REQUIRE(testCfg.internalStore()->magic == GATAS::BinaryStore::MAGIC);
        REQUIRE(testCfg.internalStore()->gatasId == 1234);
    }

    SECTION("Init internalstore just once")
    {
        get_rand_64_SET(1234);
        Config testCfg{bus, volatileStore, permanentStore, binaryStore, DEFAULT_GATAS_CONFIG};
        testCfg.postConstruct();
        REQUIRE(testCfg.internalStore()->gatasId == 1234);

        get_rand_64_SET(12345);
        testCfg.postConstruct();
        REQUIRE(testCfg.internalStore()->gatasId == 1234);
    }
}
