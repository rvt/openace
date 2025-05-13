#pragma once

#include "constants.hpp"

#include "etl/string.h"
#include "etl/map.h"
#include "etl/vector.h"

namespace OpenAce
{

    /*
     * Result after the POST_CONSTRUCT call to indicate the status of the module, eg what happened and if the module is in a good state to run
     */
    enum class PostConstruct : uint8_t
    {
        NA = 0,                     // No data yet avaiulable, use in the web modules
        OK = 1,                     // Module is ready to be started
        FAILED = 2,                 // Generale failure
        MEMORY = 3,                 // Memory allocation issues
        DEP_NOT_FOUND = 4,          // A dependency was needed but not loaded
        XQUEUE_ERROR = 5,           // FreeRTOS X queue error
        TASK_ERROR = 6,             // task creation failure
        HARDWARE_NOT_FOUND = 7,     // Hardware  not found
        HARDWARE_ERROR = 8,         // Hardware  found but not working correctly
        NETWORK_ERROR = 9,          // Netowrk issue
        CONFIG_ERROR = 10,          // Configuration issue (not found incorrect etc...)
        TIMER_ERROR = 11,           // task creation failure
        MUTEX_ERROR = 12,           // FreeRTOS X queue error
        HARDWARE_NOT_CONFIGURED = 13 // Indicate there was no configuration found
    };

    enum class PinType : uint8_t
    {
        UNKNOWN,
        SPI,
        CLK,
        MOSI,
        MISO,
        RST,
        TX,
        RX,
        BUSY,
        CS,
        O0,
        I0,
        DIO1,
        P0,
        P1,
        P2,
        AD0
    };

    // PinType alias to uint8_t
    PinType stringToPinType(const char *str);

    /**
     * Typee of aircraft
     * MUST follow Flarm's DataPort spec. If this is changed, DataPort / Flarm must be updated
     * Should we support : https://openskynetwork.github.io/opensky-api/rest.html ??
     */
    enum class AircraftCategory : uint8_t
    {
        Unknown = 0x00,
        GliderMotorGlider = 0x01,
        TowPlane = 0x02,
        Helicopter = 0x03,
        Skydiver = 0x04,
        DropPlane = 0x05,
        HangGlider = 0x06,
        Paraglider = 0x07,
        ReciprocatingEngine = 0x08,
        JetTurbopropEngine = 0x09,
        Reserved = 0x0a,
        Balloon = 0x0b,
        Airship = 0x0c,
        Uav = 0x0d,
        ReservedE = 0x0e,
        StaticObstacle = 0x0f
    };

    template <typename EnumType, typename MappingType>
    struct Mapping
    {
        EnumType type;
        MappingType name;
    };

    template <typename EnumType, typename MappingType, size_t Size>
    MappingType enumToString(const Mapping<EnumType, MappingType> (&mapping)[Size], EnumType enumValue, MappingType defaultValue)
    {
        for (const auto &entry : mapping)
        {
            if (entry.type == enumValue)
            {
                return entry.name;
            }
        }
        return defaultValue;
    }
    template <typename EnumType, typename MappingType, size_t Size>
    EnumType stringToEnum(const Mapping<EnumType, MappingType> (&mapping)[Size], const char *str, EnumType defaultValue)
    {
        if (!str)
            return defaultValue;
        for (const auto &entry : mapping)
        {
            if (strcasecmp(entry.name, str) == 0)
            {
                return entry.type;
            }
        }
        return defaultValue; // Return an invalid value if no match is found
    }

    // Aircrasft type to string
    const char *aircraftTypeToString(OpenAce::AircraftCategory at);

    // String to AircraftCategory
    AircraftCategory stringToAircraftCategory(const char *str);

    /**
     * @brief The AddressType enum indicates how the aircraft can be looked up and identified
     * it does not indicate over what channel the aircraft is transmitting (OGN. Flarm etc..)
     * Flarm can send a address type of 0x2 to indicate FLARM address or 0x1 as ICAO address
     * 0x00 is used as RANDOM
     */
    enum class AddressType : uint8_t
    {
        RANDOM = 0, // random ID, configured or if stealth mode is activated either on the target or own aircraft
        ICAO = 1,   // official ICAO 24-bit aircraft address
        FLARM = 2,  // fixed FLARM ID (chosen by FLARM)
        OGN = 3,    // fixed OGN ID (chosen by OGN)
        FANET = 4,  // FANET comes in like this via ADS-L
        ADSL = 5,
        RESERVED = 6, // RESERVED comes in like this via ADS-L
        UNKNOWN = 7,  // UNKNOWN
    };

    // Get a string representation of a AddressType
    const char *addressTypeToString(OpenAce::AddressType ds);

    // Maps a string into a AddressType
    OpenAce::AddressType stringToAddressType(const char *str);

    /**
     * Datasource is the internal service that provided and knows how to handle the data
     */
    enum class DataSource : uint8_t
    {
        FLARM = 0,
        ADSL = 1,
        FANET = 2,
        OGN1 = 3,
        _TRANSPROTOCOLS = 4, // Indicate maximum RADIO that can be received over low power (868MHZ etc..) used to limit array sizes
        PAW = 4,
        MODES = 5,
        ADSB = 6,
        NONE = 7,
        _ITEMS = 8   // Maximum number of items eg last item + 1
    };

    // Get a string representation of a datasource
    const char *dataSourceIntToString(uint8_t ds);
    const char *dataSourceToString(DataSource ds);
    DataSource stringToDataSource(const char *str);

    // Get a character representation of a datasource
    const char *dataSourceToChar(DataSource ds);

    // Values from https://en.wikipedia.org/wiki/Dilution_of_precision_(navigation)
    enum class pDopInterpretation : uint8_t
    {
        IDEAL = 0,     // .. 1
        EXCELLENT = 1, // .. 2
        GOOD = 2,      // ..5
        MODERATE = 5,  // ..10
        FAIR = 10,     // .. 20
        POOR = 21      // > 20
    };

    const char *DOPInterpretationToString(pDopInterpretation value);

    pDopInterpretation floatToDOPInterpretation(float dop);

    /**
     * Aircraft location message and time of reception
     * Note: GPS use a theoretical sea level estimated by a World Geodetic System (WGS84)
     */
    struct AircraftPositionInfo
    {
        uint32_t timestamp; 
        OpenAce::CallSign callSign;

        AircraftAddress address;
        AddressType addressType;
        DataSource dataSource;
        AircraftCategory aircraftType;
        bool stealth;  // Privacy/Stealth option
        bool noTrack;  // Tracking info (heading) unknown
        bool airborne; // Is the aircraft airborne
        float lat;
        float lon;
        int16_t altitudeWgs84; // Altitude above WGS84 ellipsoid in meters
        float verticalSpeed;   // in m/s
        float groundSpeed;     // in m/s
        int16_t course;        // 0..359
        float hTurnRate;       // deg/s Turn rate in the horizontal plane

        // These can be used by received to understand where the target is relative to ownship
        uint32_t distanceFromOwn; // Distance to ownship in meters,
        int32_t relNorthFromOwn;  // relNorth to ownship in meters
        int32_t relEastFromOwn;   // relEast to ownship in meters
        int16_t bearingFromOwn;   // Bearing to ownship in degrees

        AircraftPositionInfo(uint32_t timestamp_, OpenAce::CallSign callSign_, AircraftAddress address_, AddressType addressType_, DataSource dataSource_, AircraftCategory aircraftType_, bool stealth_, bool noTrack_, bool airborne_, float lat_, float lon_, int32_t altitudeWgs84_, float verticalSpeed_, float groundSpeed_, int16_t course_, float hTurnRate_, uint32_t distanceFromOwn_, int32_t relNorth_, int32_t relEast_, int16_t bearingFromOwn_)
            : timestamp(timestamp_), callSign(callSign_), address(address_), addressType(addressType_), dataSource(dataSource_), aircraftType(aircraftType_), stealth(stealth_), noTrack(noTrack_), airborne(airborne_), lat(lat_), lon(lon_), altitudeWgs84(altitudeWgs84_), verticalSpeed(verticalSpeed_), groundSpeed(groundSpeed_), course(course_), hTurnRate(hTurnRate_), distanceFromOwn(distanceFromOwn_), relNorthFromOwn(relNorth_), relEastFromOwn(relEast_), bearingFromOwn(bearingFromOwn_)
        {
        }
        // Default constructor
        AircraftPositionInfo() : timestamp(0), callSign(""), address(0), addressType(AddressType::RANDOM), dataSource(DataSource::NONE), aircraftType(AircraftCategory::Unknown), stealth(false), noTrack(false), airborne(false), lat(0), lon(0), altitudeWgs84(0), verticalSpeed(0), groundSpeed(0), course(0), hTurnRate(0), distanceFromOwn(INT32_MIN), relNorthFromOwn(INT32_MIN), relEastFromOwn(INT32_MIN), bearingFromOwn(INT16_MIN)
        {
        }

        AircraftPositionInfo(AircraftAddress address_) : address(address_)
        {
        }
    };

    struct OwnshipPositionInfo
    {
        uint32_t timestamp; // Timestamp when the position was received
        bool airborne;                 // Is the aircraft airborne, can this be taken from GS? It can be rare under normal situations that GS is low, even though we are flying (large headwind??)
        float lat;
        float lon;
        int16_t altitudeWgs84;   // Altitude above WGS84 ellipsoid in meters
        float verticalSpeed;     // in m/s
        float groundSpeed;       // in m/s
        float course;            // 0..359
        float hTurnRate;         // deg/s Turn rate in the horizontal plane
        float velocityNorth;     // North velocity in m/s
        float velocityEast;      // East velocity in m/s
        int16_t heightEgm96;     // Height above egm96, eg MSL
        int16_t geoidSeparation; // The distance from the surface of an ellipsoid to the surface of the geoid.
    };

    namespace Config
    {
        /**
         * Object that specifies how to configure OpenACE for this aircraft
         */
        struct OpenAceConfiguration
        {
            AircraftCategory category;
            AddressType addressType;
            AircraftAddress address;
            bool stealth;
            bool noTrack;
            etl::vector<DataSource, static_cast<uint8_t>(OpenAce::DataSource::_TRANSPROTOCOLS)> protocols;
        };

        /**
         * SSDPassword structure
         */
        struct WifiNamePassword
        {
            OpenAce::SsidOrPasswdStr ssid;
            OpenAce::SsidOrPasswdStr password;
            WifiNamePassword() : ssid(""), password("") {}
            WifiNamePassword(const etl::istring& ssid, const etl::istring& password)
                : ssid(ssid), password(password)
            {
            }
            WifiNamePassword(const char* ssid, const char* password)
                : ssid(ssid), password(password)
            {
            }
        };

        /**
         * WifiService data
         */
        struct WifiServiceData
        {
            WifiNamePassword ap;
            bool apDisabled;
            etl::vector<WifiNamePassword, 4> clients;
        };

        /**
         * IP Adress Port Structure
         */
        struct IpPort
        {
            // For now IPv4 (may be add LwIp to case and add ip_addr_t?? does not make it very portable..)
            uint32_t ip;
            uint16_t port;
        };
    };
};
