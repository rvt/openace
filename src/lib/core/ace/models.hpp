#pragma once

#include "constants.hpp"

//#include "etl/string.h"
// #include "etl/map.h"
#include "etl/vector.h"
#include "etl/enum_type.h"

namespace GATAS
{

    /*
     * Result after the POST_CONSTRUCT call to indicate the status of the module, eg what happened and if the module is in a good state to run
     */
    enum class PostConstruct : uint8_t
    {
        NA = 0,                      // No data yet avaiulable, use in the web modules
        OK = 1,                      // Module is ready to be started
        FAILED = 2,                  // Generale failure
        MEMORY = 3,                  // Memory allocation issues
        DEP_NOT_FOUND = 4,           // A dependency was needed but not loaded
        XQUEUE_ERROR = 5,            // FreeRTOS X queue error
        TASK_ERROR = 6,              // task creation failure
        HARDWARE_NOT_FOUND = 7,      // Hardware  not found
        HARDWARE_ERROR = 8,          // Hardware  found but not working correctly
        NETWORK_ERROR = 9,           // Netowrk issue
        CONFIG_ERROR = 10,           // Configuration issue (not found incorrect etc...)
        TIMER_ERROR = 11,            // task creation failure
        MUTEX_ERROR = 12,            // FreeRTOS X queue error
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

    // Assigments follows closly GDL90 specification
    struct AircraftCategory
    {
        enum enum_type : uint8_t
        {
            UNKNOWN = 0,
            LIGHT = 1,       // < 15.500lbs
            SMALL = 2,       //   15.500lbs to 75.000lbs
            LARGE = 3,       // < 75.000lbs to 300.000lbs
            HIGH_VORTEX = 4, // Such as Boing 757
            HEAVY_ICAO = 5,  // > 300.000lbs
            AEROBATIC = 6,
            ROTORCRAFT = 7,
            // UnAssigned
            GLIDER = 9,
            LIGHT_THAN_AIR = 10, 
            SKY_DIVER = 11,
            ULTRA_LIGHT_FIXED_WING = 12, // < 1500 lbs Fixed Wing only
            // UnAssigned
            UN_MANNED = 14,
            SPACE_VEHICLE = 15,
            // UnAssigned
            SURFACE_EMERGENCY_VEHICLE = 17,
            SURFACE_VEHICLE = 18, // SurfaceVehicle
            POINT_OBSTACLE = 19,
            CLUSTER_OBSTACLE = 20,
            LINE_OBSTACLE = 21,
            // .. reserved to 39
            // From below are custom values 
            GYROCOPTER = 40,
            HANG_GLIDER = 41,
            PARA_GLIDER = 42,
            DROP_PLANE = 43,
            MILITARY = 44,
        };

        ETL_DECLARE_ENUM_TYPE(AircraftCategory, uint8_t)
        ETL_ENUM_TYPE(UNKNOWN, "Unknown")
        ETL_ENUM_TYPE(LIGHT, "Light")
        ETL_ENUM_TYPE(SMALL, "Small")
        ETL_ENUM_TYPE(LARGE, "Large")
        ETL_ENUM_TYPE(HIGH_VORTEX, "High Vortex")
        ETL_ENUM_TYPE(HEAVY_ICAO, "Heavy (ICAO)")
        ETL_ENUM_TYPE(AEROBATIC, "Aerobatic")
        ETL_ENUM_TYPE(ROTORCRAFT, "Helicopter")
        // Unassigned
        ETL_ENUM_TYPE(GLIDER, "Glider")
        ETL_ENUM_TYPE(LIGHT_THAN_AIR, "Balloon")
        ETL_ENUM_TYPE(SKY_DIVER, "Sky Diver")
        ETL_ENUM_TYPE(ULTRA_LIGHT_FIXED_WING, "Ultra Light")
        // Unassigned
        ETL_ENUM_TYPE(UN_MANNED, "Unmanned aerial vehicle")
        ETL_ENUM_TYPE(SPACE_VEHICLE, "Space Vehicle")
        // Unassigned
        ETL_ENUM_TYPE(SURFACE_EMERGENCY_VEHICLE, "Surface Emergency Vehicle")
        ETL_ENUM_TYPE(SURFACE_VEHICLE, "Surface Vehicle")
        ETL_ENUM_TYPE(POINT_OBSTACLE, "Point Obstacle")
        ETL_ENUM_TYPE(CLUSTER_OBSTACLE, "Cluster Obstacle")
        ETL_ENUM_TYPE(LINE_OBSTACLE, "Line Obstacle")
        // From below are custom values
        ETL_ENUM_TYPE(GYROCOPTER, "Gyrocopter")
        ETL_ENUM_TYPE(HANG_GLIDER, "Hang Glider")
        ETL_ENUM_TYPE(PARA_GLIDER, "Para Glider")
        ETL_ENUM_TYPE(DROP_PLANE, "Drop Plane")
        ETL_ENUM_TYPE(MILITARY, "Military")
        ETL_END_ENUM_TYPE
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
        {
            return defaultValue;
        }
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
    const char *aircraftTypeToString(GATAS::AircraftCategory at);

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
    const char *addressTypeToString(GATAS::AddressType ds);

    // Maps a string into a AddressType
    GATAS::AddressType stringToAddressType(const char *str);

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
        ADSB = 5,
        _ITEMS = 6,   // Maximum number of items eg last item + 1
        UNKNOWN = 255 // Note: Never use this! Unly used for stringToEnum(..)
    };

    // Get a string representation of a datasource
    const char *dataSourceIntToString(uint8_t ds);
    const char *toString(DataSource ds);
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
        GATAS::CallSign callSign;

        AircraftAddress address;
        AddressType addressType;
        DataSource dataSource;
        AircraftCategory aircraftType;
        bool stealth;  // Privacy/Stealth option
        bool noTrack;  // this aircraft does not want to be tracked
        bool airborne; // Is the aircraft airborne
        float lat;
        float lon;
        int16_t ellipseHeight; // Altitude above the GeoId (MSL) in meters. For aircraft where altitude is based from BARO, this is an estimate
        float verticalSpeed;   // in m/s
        float groundSpeed;     // in m/s
        int16_t course;        // 0..359
        float hTurnRate;       // deg/s Turn rate in the horizontal plane

        // These can be used by received to understand where the target is relative to ownship
        uint32_t distanceFromOwn; // Distance to ownship in meters,
        int32_t relNorthFromOwn;  // relNorth to ownship in meters
        int32_t relEastFromOwn;   // relEast to ownship in meters
        // TODO: Add relative vertical?
//        int16_t bearingFromOwn;   // Bearing to ownship in degrees, currently only used in AntennaRadiationPattern?

        AircraftPositionInfo(uint32_t timestamp_, GATAS::CallSign callSign_, AircraftAddress address_, AddressType addressType_, DataSource dataSource_, AircraftCategory aircraftType_, bool stealth_, bool noTrack_, bool airborne_, float lat_, float lon_, int32_t ellipseHeight_, float verticalSpeed_, float groundSpeed_, int16_t course_, float hTurnRate_, uint32_t distanceFromOwn_, int32_t relNorth_, int32_t relEast_/*, int16_t bearingFromOwn_*/)
            : timestamp(timestamp_), callSign(callSign_), address(address_), addressType(addressType_), dataSource(dataSource_), aircraftType(aircraftType_), stealth(stealth_), noTrack(noTrack_), airborne(airborne_), lat(lat_), lon(lon_), ellipseHeight(ellipseHeight_), verticalSpeed(verticalSpeed_), groundSpeed(groundSpeed_), course(course_), hTurnRate(hTurnRate_), distanceFromOwn(distanceFromOwn_), relNorthFromOwn(relNorth_), relEastFromOwn(relEast_)// , bearingFromOwn(bearingFromOwn_)
        {
        }
        // Default constructor
        AircraftPositionInfo() : timestamp(0), callSign(""), address(0), addressType(AddressType::RANDOM), dataSource(DataSource::ADSL), aircraftType(AircraftCategory::UNKNOWN), stealth(false), noTrack(false), airborne(false), lat(0), lon(0), ellipseHeight(0), verticalSpeed(0), groundSpeed(0), course(0), hTurnRate(0), distanceFromOwn(INT32_MIN), relNorthFromOwn(INT32_MIN), relEastFromOwn(INT32_MIN)// , bearingFromOwn(INT16_MIN)
        {
        }

        AircraftPositionInfo(AircraftAddress address_) : address(address_)
        {
        }
    };

    namespace Config
    {
        struct Conspicuity
        {
            AircraftAddress address;
            AircraftCategory category;
            AddressType addressType;
            bool stealth;
            bool noTrack;
        };

        /**
         * Object that specifies how to configure GATAS for this aircraft
         */
        struct GaTasConfiguration
        {
            Conspicuity conspicuity;
            etl::vector<DataSource, static_cast<uint8_t>(GATAS::DataSource::_TRANSPROTOCOLS)> protocols;
        };

        /**
         * SSDPassword structure
         */
        struct WifiNamePassword
        {
            GATAS::SsidOrPasswdStr ssid;
            GATAS::SsidOrPasswdStr password;
            WifiNamePassword() : ssid(""), password("") {}
            WifiNamePassword(const etl::istring &ssid, const etl::istring &password)
                : ssid(ssid), password(password)
            {
            }
            WifiNamePassword(const char *ssid, const char *password)
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

    struct OwnshipPositionInfo
    {
        uint32_t timestamp;              // Timestamp when the position was received
        float lat;
        float lon;
        int16_t ellipseHeight;           // Height above the Ellipsoid (WGS84) in meters. For aircraft where altitude is based from BARO, this is an estimate
        float verticalSpeed;             // in m/s
        float groundSpeed;               // in m/s
        float course;                    // 0..359
        float hTurnRate;                 // deg/s Turn rate in the horizontal plane
        float velocityNorth;             // North velocity in m/s
        float velocityEast;              // East velocity in m/s
        int16_t geoidSeparation;         // The distance from the surface of an ellipsoid to the surface of the geoid.
        bool airborne;                   // Is the aircraft airborne, can this be taken from GS? It can be rare under normal situations that GS is low, even though we are flying (large headwind??)
        Config::Conspicuity conspicuity; // Configuration for this aircraft, used to send out the correct data
        int16_t heightMsl() const
        {
            return ellipseHeight - geoidSeparation;
        }

        static const uint8_t COBS_SIZE = 26;
    };

    enum class Modulation : uint8_t
    {
        NONE = 0,
        GFSK = 1,
        LORA = 2,
    };

    const char *modulationToString(GATAS::Modulation mode);

    /**
     * @brief Radio frame received by transceiver. Can contain both LORA and GFSK data frames
     *
     */
    struct DataFrame
    {
        uint32_t epochSeconds;
        uint32_t frequency;
        int8_t rssidBm;
        GATAS::DataSource dataSource;
        GATAS::Modulation modulation;
        uint8_t length;                         // Length of the data frame
        uint8_t data[MAXIMUM_RAW_FRAME_LENGTH]; // Data frame content

        static constexpr size_t maxFrameDataLength()
        {
            return sizeof(std::declval<DataFrame>().data);
        }
    };
};
