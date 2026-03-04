#pragma once

#include <cmath>
#include "etl/flat_map.h"
#include "etl/string.h"
#include "etl/vector.h"
#include "etl/string_stream.h"
#include "poolallocator.hpp"

constexpr float KN_TO_MS = 0.514444444f;            // knots to meter/sec
constexpr float MS_TO_KN = 1.0f / KN_TO_MS;         // meter/sec to knots
constexpr float MS_TO_FTPMIN = 196.850394f;         // meter/sec to feet/min
constexpr float FTPMIN_TO_MS = 1.0f / MS_TO_FTPMIN; // feet/min to meter/sec
constexpr float MS_TO_KPH = 3.6f;                   // meter/sec to km/h
constexpr float KPH_TO_MS = 1.f / MS_TO_KPH;        //  km/hto meter/sec
constexpr float MS_TO_MPH = 2.23693629f;            // meter/sec to miles/h
constexpr float MS_TO_DMPS = 10.f;                  // meter/sec to decmeter/sec
constexpr float DPMS_TO_MS = 1.f / MS_TO_DMPS;      // decmeter/sec to meter/sec
constexpr float FT_TO_M = 0.3048f;                  // feet to meter
constexpr float M_TO_FT = 1.0f / FT_TO_M;           // feet to meter
constexpr float DEG_TO_RADS = M_PI / 180.f;         // degrees to radians
constexpr float RADS_TO_DEG = 180.f / M_PI;         // degrees to radians
constexpr float DIAMETER_EARTH_M = 6371640;         // Radius of the earth in km

#if !defined(M_TWOPI)
constexpr float M_TWOPI = 2.0f * M_PI; // 2 x PI
#endif

namespace GATAS
{
    constexpr uint8_t MODULE_NAME_LENGTH = 16;
    constexpr uint8_t MANCHESTER = 2;
    constexpr uint8_t NMEA_MAX_LENGTH = 84;
    constexpr uint8_t MAX_CALLSIGN_LENGTH = 12;
    constexpr uint8_t RADIO_MAX_TX_GFSK_FRAME_LENGTH = 32; // Maximum length in bytes for a GFSK frame we expect to send
    constexpr uint8_t MAX_AIRCRAFT_CONFIG = 10;            // See also session.js line 58
    constexpr uint8_t MAX_LENGTH_ADSB = 33;
    constexpr float GROUNDSPEED_CONSIDERING_AIRBORN = 3.f; // groundspeed 3m/s == 5.8kt is considered beeing airborn

    using NMEAString = etl::string<NMEA_MAX_LENGTH>;   // NMEA sentence
    using IcaoAddress = etl::string<8>;                // ICAO Address as hex string
    using CallSign = etl::string<MAX_CALLSIGN_LENGTH>; // ICAO Address as hex string
    using ADSBString = etl::string<MAX_LENGTH_ADSB>;   // ADSB message, similar as from dump1090
    using ConfigString = etl::string<64>;              // Maximum length of value from the configuration
    using Modulename = etl::string<17>;
    using ConfigPathString = etl::string<65>; // Maximum path length of value from the configuration including api name and module name
    using GDLData = etl::vector<uint8_t, 52>; // GDLXX Message
    using AircraftAddress = uint32_t;         // ICAO code, FLARM ID or OGN ID
    using SsidOrPasswdStr = etl::string<32>;
    // 32 Byte is a decoded manchester frame
    // 64 is an manchester frame
    // 160 byte are LORA ADSL-H Frames
    using GlobalPoolConfiguration = MultiPoolAllocator<PoolSpec<32, 8>, PoolSpec<64, 4>, PoolSpec<160, 4>>;

    enum class PinType : uint8_t;
    using PinTypeMap = etl::flat_map<PinType, int8_t, 8>;

    constexpr etl::format_spec ICAO_HEX_FORMAT = etl::format_spec{}.hex().width(6).upper_case(true);
    constexpr etl::format_spec RESET_FORMAT = etl::format_spec{};
}
