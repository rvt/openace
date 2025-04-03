#pragma once

#include <math.h>
#include "etl/flat_map.h"
#include "etl/string.h"
#include "etl/vector.h"
#include "etl/string_stream.h"

constexpr float KN_TO_MS = 0.514444444f;            // knots to meter/sec
constexpr float MS_TO_FTPMIN = 196.850394f;         // meter/sec to feet/min
constexpr float FTPMIN_TO_MS = 1.0f / MS_TO_FTPMIN; // feet/min to meter/sec
constexpr float MS_TO_KN = 1.0f / KN_TO_MS;         // meter/sec to knots
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

namespace OpenAce
{
    constexpr uint8_t MODULE_NAME_LENGTH = 16;
    constexpr uint8_t MANCHESTER = 2;
    constexpr uint8_t NMEA_MAX_LENGTH = 84;                                                    // Maximum length NMEA sentence + 1 for null terminator
    constexpr uint8_t RADIO_MAX_FRAME_WORD_LENGTH = 7;                                         // OGN:26 FLARM:24 Maximum length expected for a frame received via Radio (OGN/ Flarm, Fannet etc...) This includes address bytes + Some zero's for parity calculations
    constexpr uint8_t RADIO_MAX_FRAME_LENGTH = RADIO_MAX_FRAME_WORD_LENGTH * sizeof(uint32_t); // Maximum length expected for a frame received via Radio (OGN/ Flarm, Fannet etc...) This includes address bytes + Some zero's for parity calculations
    constexpr uint8_t MAX_LENGTH_ADSB = 33;
    constexpr uint8_t MAX_LORA_MSG_SIZE = 128;
    constexpr float GROUNDSPEED_CONSIDERING_AIRBORN = 15.f; // groundspeed > 25ms is considered beeing airborn
    // @todo: added one extra word because 'somwhere' I think there is an issue where we corrupt the stack
    using NMEAString = etl::string<NMEA_MAX_LENGTH>; // NMEA sentence
    using IcaoAddress = etl::string<8>;              // ICAO Address as hex string
    using ADSBString = etl::string<MAX_LENGTH_ADSB>; // ADSB message, similar as from dump1090
    using ConfigString = etl::string<25>;            // Maximum length of value from the configuration
    using Modulename = etl::string<17>;
    using ConfigPathString = etl::string<65>; // Maximum path length of value from the configuration including api name and module name
    using GDLData = etl::vector<uint8_t, 48>; // GDLXX Message
    using AircraftAddress = uint32_t;         // ICAO code, FLARM ID or OGN ID
    using TxPacketType = etl::array<uint8_t, OpenAce::RADIO_MAX_FRAME_LENGTH>;
    using TxFrameType = etl::array<uint8_t, OpenAce::RADIO_MAX_FRAME_LENGTH * MANCHESTER>;
    using SsidOrPasswdStr = etl::string<20>;

    enum class PinType : uint8_t;
    using PinTypeMap = etl::flat_map<PinType, int8_t, 8>;

    constexpr etl::format_spec ICAO_HEX_FORMAT = etl::format_spec{}.hex().width(6).upper_case(true);
    constexpr etl::format_spec RESET_FORMAT = etl::format_spec{};
}
