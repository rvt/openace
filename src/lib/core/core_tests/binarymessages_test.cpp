#include <catch2/catch_test_macros.hpp>

#include <etl/array.h>

#include "binarymessages.hpp"

TEST_CASE("Aircraft configuration exposes wifi mode in reserved byte", "[binarymessages]")
{
    etl::array<uint32_t, 2> addresses = {0x010203, 0x040506};
    etl::span<uint32_t> addressSpan(addresses.data(), addresses.size());
    const size_t rawSize = BinaryMessages::serializeAircraftConfigurationSizeV2().items(addresses.size());
    uint8_t buffer[64] = {};

    etl::bit_stream_writer ncWriter(buffer, rawSize, etl::endian::big);
    BinaryMessages::serializeAircraftConfigurationV2(ncWriter, 0x01020304, 0x050607, addressSpan, 0x0a0b0c0d, 0x0e0f10, GATAS::WifiMode::NC);
    REQUIRE(buffer[0] == BinaryMessages::DataType(BinaryMessages::DataType::AIRCRAFT_CONFIGURATIONS_V2).get_value());
    REQUIRE((buffer[1] & BinaryMessages::AIRCRAFT_CONFIGURATION_WIFI_MODE_MASK) == static_cast<uint8_t>(GATAS::WifiMode::NC));

    etl::bit_stream_writer apWriter(buffer, rawSize, etl::endian::big);
    BinaryMessages::serializeAircraftConfigurationV2(apWriter, 0x01020304, 0x050607, addressSpan, 0x0a0b0c0d, 0x0e0f10, GATAS::WifiMode::AP);
    REQUIRE((buffer[1] & BinaryMessages::AIRCRAFT_CONFIGURATION_WIFI_MODE_MASK) == static_cast<uint8_t>(GATAS::WifiMode::AP));

    etl::bit_stream_writer clientWriter(buffer, rawSize, etl::endian::big);
    BinaryMessages::serializeAircraftConfigurationV2(clientWriter, 0x01020304, 0x050607, addressSpan, 0x0a0b0c0d, 0x0e0f10, GATAS::WifiMode::CLIENT);
    REQUIRE((buffer[1] & BinaryMessages::AIRCRAFT_CONFIGURATION_WIFI_MODE_MASK) == static_cast<uint8_t>(GATAS::WifiMode::CLIENT));
}

TEST_CASE("WiFi mode control frame only accepts AP and CLIENT requests", "[binarymessages]")
{
    {
        uint8_t frame[] = {
            BinaryMessages::DataType(BinaryMessages::DataType::SET_WIFI_MODE_V1).get_value(),
            static_cast<uint8_t>(GATAS::WifiMode::CLIENT),
        };
        etl::bit_stream_reader reader(frame, sizeof(frame), etl::endian::big);
        GATAS::WifiMode wifiMode = GATAS::WifiMode::NC;
        REQUIRE(BinaryMessages::deserializeSetWifiModeV1(reader, wifiMode));
        REQUIRE(wifiMode == GATAS::WifiMode::CLIENT);
    }

    {
        uint8_t frame[] = {
            BinaryMessages::DataType(BinaryMessages::DataType::SET_WIFI_MODE_V1).get_value(),
            static_cast<uint8_t>(GATAS::WifiMode::AP),
        };
        etl::bit_stream_reader reader(frame, sizeof(frame), etl::endian::big);
        GATAS::WifiMode wifiMode = GATAS::WifiMode::NC;
        REQUIRE(BinaryMessages::deserializeSetWifiModeV1(reader, wifiMode));
        REQUIRE(wifiMode == GATAS::WifiMode::AP);
    }

    {
        uint8_t frame[] = {
            BinaryMessages::DataType(BinaryMessages::DataType::SET_WIFI_MODE_V1).get_value(),
            static_cast<uint8_t>(GATAS::WifiMode::NC),
        };
        etl::bit_stream_reader reader(frame, sizeof(frame), etl::endian::big);
        GATAS::WifiMode wifiMode = GATAS::WifiMode::AP;
        REQUIRE_FALSE(BinaryMessages::deserializeSetWifiModeV1(reader, wifiMode));
    }
}
