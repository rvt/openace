
#pragma once

#include <etl/bit_stream.h>
#include <etl/algorithm.h>
#include <etl/span.h>
#include <etl/absolute.h>
#include <etl/to_arithmetic.h>
#include "lib_crc.hpp"
#include "coreutils.hpp"
#include "models.hpp"
#include "ace/measure.hpp"

class BinaryMessages
{
public:
    /**
     * Object that can be used by binary messages to indicate how many bytes it will consume
     * A function 'items' is provided to calculate the number of items that can be expected to calculate
     * total length.
     */
    struct SizeType
    {
        size_t base;
        size_t size;
        constexpr size_t items(size_t items)
        {
            return base + size * items;
        };
    };

    // Assigments follows closly GDL90 specification
    struct DataType
    {
        enum enum_type : uint8_t
        {
            AIRCRAFT_POSITION_TYPE_V1 = 1,    // BinaryMessage type of an aircraft other than our own, this can be injexted in the system to process and display
            AIRCRAFT_POSITION_REQUEST_V1 = 2, // Binary message of a request for other aircraft from gatasConnect
            AIRCRAFT_CONFIGURATIONS_V1 = 3,   // Current GATAS COnfiguration
            SET_ICAO_ADDRESS_V1 = 4           // Set a new aircraft configuration based on hexcode, this is like if you set from teh AI a other aircraft
        };

        ETL_DECLARE_ENUM_TYPE(DataType, uint8_t)
        ETL_ENUM_TYPE(AIRCRAFT_POSITION_TYPE_V1, "Aircraft Data")
        ETL_ENUM_TYPE(AIRCRAFT_POSITION_REQUEST_V1, "Conspcuity Data Request")
        ETL_ENUM_TYPE(AIRCRAFT_CONFIGURATIONS_V1, "Current GATAS Configuration")
        ETL_ENUM_TYPE(SET_ICAO_ADDRESS_V1, "Set new aircraft from configuration")
        ETL_END_ENUM_TYPE
    };

    /**
     * Read Aircraft Position Info from a bit stream reader
     */
    static GATAS::AircraftPositionInfo deserializeAircraftPositionV1(float ownshipLat, float ownshipLon, etl::bit_stream_reader &reader)
    {
        auto timeStamp = CoreUtils::timeUs32();
        auto type = reader.read_unchecked<uint8_t>(8U);
        if (type != DataType(DataType::AIRCRAFT_POSITION_TYPE_V1).get_value())
        {
            return GATAS::AircraftPositionInfo();
        }
        uint32_t addressRaw = reader.read_unchecked<uint32_t>(24U);
        uint8_t addressTypeIdx = reader.read_unchecked<uint8_t>(8U);
        uint8_t dataSourceIdx = reader.read_unchecked<uint8_t>(8U);
        float lat = static_cast<float>(reader.read_unchecked<int32_t>(32U)) / 1E7;
        float lon = static_cast<float>(reader.read_unchecked<int32_t>(32U)) / 1E7;
        int16_t heightHAE = reader.read_unchecked<int16_t>(16U) - 100; // Aircraft message needs to be in ellipsoid
        float track = static_cast<float>(reader.read_unchecked<uint8_t>(8U)) * (360.f / 255.f);
        float turnRate = static_cast<float>(reader.read_unchecked<int8_t>(8U)) / 5.0f;
        float groundSpeed = static_cast<float>(reader.read_unchecked<uint16_t>(16U)) / 100.f;
        float verticalRate = static_cast<float>(reader.read_unchecked<int16_t>(16U)) / 1024.f;
        uint8_t aircraftCategoryIdx = reader.read_unchecked<uint8_t>(8U);

        uint8_t callSignLen = etl::min(GATAS::MAX_CALLSIGN_LENGTH, reader.read_unchecked<uint8_t>(8U));
        char callSignBuffer[GATAS::MAX_CALLSIGN_LENGTH + 1] = {0};
        for (int i = 0; i < callSignLen; ++i)
        {
            callSignBuffer[i] = static_cast<char>(reader.read_unchecked<uint8_t>(8));
        }
        auto rel = CoreUtils::getDistanceRelNorthRelEastInt(ownshipLat, ownshipLon, lat, lon);

        return GATAS::AircraftPositionInfo(
            timeStamp,
            GATAS::CallSign(callSignBuffer),
            static_cast<GATAS::AircraftAddress>(addressRaw),
            static_cast<GATAS::AddressType>(addressTypeIdx),
            static_cast<GATAS::DataSource>(dataSourceIdx),
            static_cast<GATAS::AircraftCategory>(aircraftCategoryIdx),
            false, // stealth
            false, // noTrack
            true,  // airborne
            lat,
            lon,
            heightHAE,
            verticalRate,
            groundSpeed,
            track,
            turnRate,
            rel.distance,
            rel.relNorth,
            rel.relEast);
    }

    /**
     * Create a bitstream from an OwnshipPositionInfo to be send to gatasServer witg a requets
     * to send back aircraft
     */
    static void serializeOwnshipPositionV1(etl::bit_stream_writer &writer, const GATAS::OwnshipPositionInfo &ownship)
    {
        writer.write_unchecked(DataType(DataType::AIRCRAFT_POSITION_REQUEST_V1).get_value(), 8U);
        writer.write_unchecked(CoreUtils::secondsSinceEpoch(), 32U);
        writer.write_unchecked(ownship.conspicuity.icaoAddress, 24U);
        writer.write_unchecked(static_cast<uint8_t>(ownship.conspicuity.addressType), 8U);
        writer.write_unchecked(GATAS::AircraftCategory(ownship.conspicuity.category).get_value(), 8U);
        writer.write_unchecked(static_cast<int32_t>(ownship.lat * 1E7 + 0.5f), 32U);
        writer.write_unchecked(static_cast<int32_t>(ownship.lon * 1E7 + 0.5f), 32U);
        writer.write_unchecked(ownship.ellipseHeight + 100, 16U); // Aircraft message needs to be in ellipsoid
        writer.write_unchecked(static_cast<uint8_t>(ownship.course / (360.f / 255.f)), 8U);
        writer.write_unchecked(static_cast<int8_t>(ownship.hTurnRate * 5.0f), 8U);
        writer.write_unchecked(static_cast<uint16_t>(ownship.groundSpeed * 10.f), 16U);
        writer.write_unchecked(static_cast<int16_t>(ownship.verticalSpeed * 100.f), 16U);
    }

    constexpr static BinaryMessages::SizeType serializeOwnshipPositionSizeV1()
    {
        size_t size = 1 + 4 + 3 + 1 + 1 + 4 + 4 + 2 + 1 + 1 + 2 + 2;
        return BinaryMessages::SizeType {
            .base = 0,
            .size = size};
    }

    /**
     * Inform gatasServer the configuration of this device
     */
    static void serializeAircraftConfigurationV1(etl::bit_stream_writer &writer, uint32_t gatasId, uint32_t currentAddress, const etl::span<uint32_t> &addresses)
    {
        writer.write_unchecked(DataType(DataType::AIRCRAFT_CONFIGURATIONS_V1).get_value(), 8U);
        writer.write_unchecked(gatasId, 32U);
        writer.write_unchecked(currentAddress, 24U);

        // options how to set the addres as a response
        // Examples could be:
        // 0 Not allowed at all
        // 1 Only set by user request, ia app or via website
        // 2 By automation
        writer.write_unchecked(0, 8U);

        writer.write_unchecked(addresses.size(), 8U);
        for (auto &addr : addresses)
        {
            writer.write_unchecked(addr, 24U);
        }
    }

    constexpr static BinaryMessages::SizeType serializeAircraftConfigurationSizeV1()
    {
        return BinaryMessages::SizeType {
            .base = 1 + 4 + 3 + 1 + 1, // By default we will use 4 bytes 10
            .size = 3                  // For each additional item 3 bytes
        };
    }

    static uint32_t deserializeSetIcaoAddressV1(etl::bit_stream_reader &reader)
    {
        auto type = reader.read_unchecked<uint8_t>(8U);
        if (type != DataType(DataType::SET_ICAO_ADDRESS_V1).get_value())
        {
            return 0x00;
        }
        return reader.read_unchecked<uint32_t>(24U);
    }

    /**
     *
     */
    static GATAS::AircraftCategory safeMapAircraftCategoryToType(uint8_t category)
    {
        // won't work well due to not mapping to unknown correctly
        // return GATAS::AircraftCategory(category);
        // clang-format off
        switch (category)
        {
            // Standard categories
            case 0:  return GATAS::AircraftCategory::UNKNOWN;
            case 1:  return GATAS::AircraftCategory::LIGHT;
            case 2:  return GATAS::AircraftCategory::SMALL;
            case 3:  return GATAS::AircraftCategory::LARGE;
            case 4:  return GATAS::AircraftCategory::HIGH_VORTEX;
            case 5:  return GATAS::AircraftCategory::HEAVY_ICAO;
            case 6:  return GATAS::AircraftCategory::AEROBATIC;
            case 7:  return GATAS::AircraftCategory::ROTORCRAFT;
            case 9:  return GATAS::AircraftCategory::GLIDER;
            case 10: return GATAS::AircraftCategory::LIGHT_THAN_AIR;
            case 11: return GATAS::AircraftCategory::SKY_DIVER;
            case 12: return GATAS::AircraftCategory::ULTRA_LIGHT_FIXED_WING;
            case 14: return GATAS::AircraftCategory::UN_MANNED;
            case 15: return GATAS::AircraftCategory::SPACE_VEHICLE;
            case 17: return GATAS::AircraftCategory::SURFACE_EMERGENCY_VEHICLE;
            case 18: return GATAS::AircraftCategory::SURFACE_VEHICLE;
            case 19: return GATAS::AircraftCategory::POINT_OBSTACLE;
            case 20: return GATAS::AircraftCategory::CLUSTER_OBSTACLE;
            case 21: return GATAS::AircraftCategory::LINE_OBSTACLE;
            case 40: return GATAS::AircraftCategory::GYROCOPTER;
            case 41: return GATAS::AircraftCategory::HANG_GLIDER;
            case 42: return GATAS::AircraftCategory::PARA_GLIDER;
            case 43: return GATAS::AircraftCategory::DROP_PLANE;
            case 44: return GATAS::AircraftCategory::MILITARY;
            case 8:
            case 13:
            case 16:
            default:return GATAS::AircraftCategory::UNKNOWN;
        }
        // clang-format on
    }

    static GATAS::AircraftCategory mapAircraftCategoryToType(const etl::string_view category)
    {
        // Map using ETL_ENUM_TYPE string representations
        // clang-format off
        if (category == GATAS::AircraftCategory(GATAS::AircraftCategory::UNKNOWN).c_str()) return GATAS::AircraftCategory::UNKNOWN;
        if (category == GATAS::AircraftCategory(GATAS::AircraftCategory::LIGHT).c_str()) return GATAS::AircraftCategory::LIGHT;
        if (category == GATAS::AircraftCategory(GATAS::AircraftCategory::SMALL).c_str()) return GATAS::AircraftCategory::SMALL;
        if (category == GATAS::AircraftCategory(GATAS::AircraftCategory::LARGE).c_str()) return GATAS::AircraftCategory::LARGE;
        if (category == GATAS::AircraftCategory(GATAS::AircraftCategory::HIGH_VORTEX).c_str()) return GATAS::AircraftCategory::HIGH_VORTEX;
        if (category == GATAS::AircraftCategory(GATAS::AircraftCategory::HEAVY_ICAO).c_str()) return GATAS::AircraftCategory::HEAVY_ICAO;
        if (category == GATAS::AircraftCategory(GATAS::AircraftCategory::AEROBATIC).c_str()) return GATAS::AircraftCategory::AEROBATIC;
        if (category == GATAS::AircraftCategory(GATAS::AircraftCategory::ROTORCRAFT).c_str()) return GATAS::AircraftCategory::ROTORCRAFT;
        if (category == GATAS::AircraftCategory(GATAS::AircraftCategory::GLIDER).c_str()) return GATAS::AircraftCategory::GLIDER;
        if (category == GATAS::AircraftCategory(GATAS::AircraftCategory::LIGHT_THAN_AIR).c_str()) return GATAS::AircraftCategory::LIGHT_THAN_AIR;
        if (category == GATAS::AircraftCategory(GATAS::AircraftCategory::SKY_DIVER).c_str()) return GATAS::AircraftCategory::SKY_DIVER;
        if (category == GATAS::AircraftCategory(GATAS::AircraftCategory::ULTRA_LIGHT_FIXED_WING).c_str()) return GATAS::AircraftCategory::ULTRA_LIGHT_FIXED_WING;
        if (category == GATAS::AircraftCategory(GATAS::AircraftCategory::UN_MANNED).c_str()) return GATAS::AircraftCategory::UN_MANNED;
        if (category == GATAS::AircraftCategory(GATAS::AircraftCategory::SPACE_VEHICLE).c_str()) return GATAS::AircraftCategory::SPACE_VEHICLE;
        if (category == GATAS::AircraftCategory(GATAS::AircraftCategory::SURFACE_EMERGENCY_VEHICLE).c_str()) return GATAS::AircraftCategory::SURFACE_EMERGENCY_VEHICLE;
        if (category == GATAS::AircraftCategory(GATAS::AircraftCategory::SURFACE_VEHICLE).c_str()) return GATAS::AircraftCategory::SURFACE_VEHICLE;
        if (category == GATAS::AircraftCategory(GATAS::AircraftCategory::POINT_OBSTACLE).c_str()) return GATAS::AircraftCategory::POINT_OBSTACLE;
        if (category == GATAS::AircraftCategory(GATAS::AircraftCategory::CLUSTER_OBSTACLE).c_str()) return GATAS::AircraftCategory::CLUSTER_OBSTACLE;
        if (category == GATAS::AircraftCategory(GATAS::AircraftCategory::LINE_OBSTACLE).c_str()) return GATAS::AircraftCategory::LINE_OBSTACLE;
        if (category == GATAS::AircraftCategory(GATAS::AircraftCategory::GYROCOPTER).c_str()) return GATAS::AircraftCategory::GYROCOPTER;
        if (category == GATAS::AircraftCategory(GATAS::AircraftCategory::HANG_GLIDER).c_str()) return GATAS::AircraftCategory::HANG_GLIDER;
        if (category == GATAS::AircraftCategory(GATAS::AircraftCategory::PARA_GLIDER).c_str()) return GATAS::AircraftCategory::PARA_GLIDER;
        if (category == GATAS::AircraftCategory(GATAS::AircraftCategory::DROP_PLANE).c_str()) return GATAS::AircraftCategory::DROP_PLANE;
        if (category == GATAS::AircraftCategory(GATAS::AircraftCategory::MILITARY).c_str()) return GATAS::AircraftCategory::MILITARY;
        // clang-format on

        // Fallback to numeric value
        if (isdigit(category.front()))
        {
            return safeMapAircraftCategoryToType(etl::to_arithmetic<uint8_t>(category));
        }

        return GATAS::AircraftCategory::UNKNOWN;
    }

    /**
     * Generate a checksuom for Bimnary Messages
     */
    static uint16_t binaryMsgChecksum(etl::span<uint8_t> packet)
    {
        uint16_t crc16 = 0xffff;
        for (auto byte : packet)
        {
            crc16 = update_crc_ccitt(crc16, byte);
        }

        return crc16;
    }
};