#pragma once

#include "constants.hpp"
#include "basemodule.hpp"
#include "models.hpp"

#include "etl/message.h"
#include "etl/message_router.h"
#include "etl/message_broker.h"
#include "etl/message_bus.h"
#include "etl/string.h"
#include "etl/set.h"
#include "etl/array.h"

namespace OpenAce
{

    /**
     * Send by ADSB Modules contains RAW ADSB message in the form of
     * *a8000fb18b51293820bcd5d0fe9c; in binary from
     */
    struct ADSBMessageBin : public etl::message<1>
    {
        static constexpr uint8_t MAX_BINARY_LENGTH = 14;
        uint8_t length;
        uint8_t data[MAX_BINARY_LENGTH];
    };

    /**
     * GPS Message, received from an attached GPS device
     */
    struct GPSSentenceMsg : public etl::message<3>
    {
        const NMEAString sentence; // Received NMEA sentence
        GPSSentenceMsg(const NMEAString &sentence_) : sentence(sentence_) {}
    };

    /**
     * NMEA Compatible message of length 83 chars including null term
     * Send to attached devices
     */
    struct DataPortMsg : public etl::message<4>
    {
        const NMEAString sentence; // Received NMEA sentence
        DataPortMsg(const NMEAString &sentence_) : sentence(sentence_) {}
    };

    /**
     * Aircraft Position of other aircraft
     */
    struct AircraftPositionMsg : public etl::message<5>
    {
        const AircraftPositionInfo position;
        int16_t rssidBm; // Received signal strength indicator in dB
        AircraftPositionMsg(const AircraftPositionInfo &position_, int16_t rssidBm_) : position(position_), rssidBm(rssidBm_) {}
        AircraftPositionMsg(const AircraftPositionInfo &position_) : position(position_), rssidBm(INT16_MIN) {}
    };

    /**
     * Aircraft Position of other aircraft from the tracker
     */
    struct TrackedAircraftPositionMsg : public etl::message<23>
    {
        const AircraftPositionInfo position;
        TrackedAircraftPositionMsg(const AircraftPositionInfo &position_) : position(position_) {}
    };

    /**
     * Aircraft Position of our ownship
     */
    struct OwnshipPositionMsg : public etl::message<6>
    {
        const OwnshipPositionInfo position;
        // Constructor
        OwnshipPositionMsg(const OwnshipPositionInfo &position_) : position(position_) {}
        // Default Constructor
        OwnshipPositionMsg() : position() {}
    };

    /**
     * Aircraft Position
     * @deprecated We might want to just use an array of future position pre estimated
     */
    // struct AircraftPositionEstimated : public etl::message<7>
    // {
    //     uint32_t timeAtEstimated; // in milliseconds when the position was estimated
    //     AircraftPositionInfo position;
    // };

    /**
     * Aircraft Position Filtered
     * THis is the same as AircraftPosition, but aircraft that are to far away or to high/low
     * are removed from these messages to reduce pressure on the cDetector
     * and the messagebus in general
     */
    // struct AircraftPositionFiltered : public etl::message<8>
    // {
    //     AircraftPositionInfo position;
    // };

    // /**
    //  * Request for estimation positions. THis will effectifly enable the estimation of the position
    //  * of the current known aircraft
    //  */
    // struct EstimationRequest : public etl::message<9>
    // {
    //     AircraftAddress address;
    //     uint8_t rate;     // updates per second request (1..5)
    //     uint8_t duration; // For how long should the estimations be active (in seconds) after the last reception
    // };

    // /**
    //  * Cancel estimation requests
    //  */
    // struct CancelEstimationRequest : public etl::message<10>
    // {
    //     AircraftAddress address;
    // };

    // struct CollisionWarning : public etl::message<11>
    // {
    //     AircraftAddress address;
    //     AddressType addressType;
    //     AircraftCategory aircraftType;
    //     DataSource dataSource;
    //     uint8_t alarmLevel;      // 0..3
    //     uint8_t secondsToImpact; // 0..30
    //     float relativeNorth;     // relative position
    //     float relativeEast;
    //     float relativeVertical; // relative Altitude above ownship in meter
    //     float track;            // Track of aircraft
    //     float turnRate;         // Turnrate of aircraft
    //     float groundSpeed;      // Groundspeed of aircraft
    //     float climbRate;        // Clibrate of aircraft
    //     bool noTrack;           // Privacy option see dataport of explanation
    // };

    struct UtcTimeMsg : public etl::message<12>
    {
        int16_t year;        // Set with full year, e.g. 2021
        int8_t month;        // 1..12
        int8_t day;          // 1..31
        int8_t hour;         // 0..23
        int8_t minute;       // 0..59
        int8_t second;       // 0..59
        int16_t millisecond; // 0..999
        // Constructor
        UtcTimeMsg(int16_t year_, int8_t month_, int8_t day_, int8_t hour_, int8_t minute_, int8_t second_, int16_t millisecond_) : year(year_), month(month_), day(day_), hour(hour_), minute(minute_), second(second_), millisecond(millisecond_) {};

        // Default constructor
        UtcTimeMsg() : year(0), month(0), day(0), hour(0), minute(0), second(0), millisecond(0) {};
    };

    struct GpsStatsMsg : public etl::message<14>
    {
        // Fix Quality
        // 0: Fix not valid, 1: GPS fix, 2: Differential GPS fix (DGNSS), SBAS, OmniSTAR VBS, Beacon, RTX in GVBS mode  3: Not applicable, 4: RTK Fixed, xFill, 5: RTK Float, OmniSTAR XP/HP, Location RTK, RTX, 6: INS Dead reckoning
        uint8_t fixQuality;         // From GGA Sentence
        uint8_t fixType;            // 1=None 2=2D or 3=3D
        uint8_t satellitesTracked;  // From GGA Sentence
        float pDop;                 // From GSA Sentence
        float hDop;                 // From GSA Sentence
        pDopInterpretation pDopInt; // Interpretation from pDop
        // COnstructor
        GpsStatsMsg(uint8_t fixQuality_, uint8_t fixType_, uint8_t satellites_tracked_, float pDop_, float hDop_) : fixQuality(fixQuality_), fixType(fixType_), satellitesTracked(satellites_tracked_), pDop(pDop_), hDop(hDop_), pDopInt(floatToDOPInterpretation(pDop_)) {};
        GpsStatsMsg() : fixQuality(0), satellitesTracked(0), pDop(255), hDop(255), pDopInt(floatToDOPInterpretation(255)) {};
    };

    struct BarometricPressureMsg : public etl::message<15>
    {
        float pressurehPa;    // Preasure in hPa (hectopascal)
        uint32_t usSinceBoot; // Time since boot
        BarometricPressureMsg(float pressurehPa_, uint32_t usSinceBoot_) : pressurehPa(pressurehPa_), usSinceBoot(usSinceBoot_) {};
        BarometricPressureMsg() : pressurehPa(0), usSinceBoot(0) {};
    };

    struct RadioRxGfskMsg : public etl::message<16>
    {
        // TODO: CHange to ETL::array
        uint32_t frame[OpenAce::RADIO_MAX_FRAME_WORD_LENGTH];
        uint32_t err[OpenAce::RADIO_MAX_FRAME_WORD_LENGTH];
        uint32_t epochSeconds;
        uint8_t length; // TODO: CHange this to length in words
        int8_t rssidBm;
        uint32_t frequency;
        OpenAce::DataSource dataSource;
        RadioRxGfskMsg(uint8_t length_, uint32_t epochSeconds_, int8_t rssidBm_, uint32_t frequency_, OpenAce::DataSource dataSource_) : epochSeconds(epochSeconds_), length(length_), rssidBm(rssidBm_), frequency(frequency_), dataSource(dataSource_)
        {
            // TODO: Decide if we need to do this
            memset(frame, 0, sizeof(frame));
            memset(err, 0, sizeof(frame));
        };
        RadioRxGfskMsg() : epochSeconds(0), length(0), rssidBm(0), frequency(0), dataSource(OpenAce::DataSource::NONE)
        {
            memset(frame, 0, sizeof(frame));
            memset(err, 0, sizeof(frame));
        };
    };

    struct RadioRxLoraMsg : public etl::message<27>
    {
        etl::vector<uint8_t, OpenAce::MAX_LORA_MSG_SIZE> frame;
        uint32_t epochSeconds;
        int8_t rssidBm;
        uint32_t frequency;
        OpenAce::DataSource dataSource;
        RadioRxLoraMsg(uint32_t epochSeconds_, int8_t rssidBm_, uint32_t frequency_, OpenAce::DataSource dataSource_) : epochSeconds(epochSeconds_), rssidBm(rssidBm_), frequency(frequency_), dataSource(dataSource_)
        {
        };
        RadioRxLoraMsg() : epochSeconds(0), rssidBm(0), frequency(0), dataSource(OpenAce::DataSource::NONE)
        {
        };
    };

    struct RadioTxPositionRequestMsg : public etl::message<2>
    {
        const Radio::RadioParameters radioParameters;
        uint8_t radioNo;
        RadioTxPositionRequestMsg(const Radio::RadioParameters &radioParameters_, uint8_t radioNo_) : radioParameters(radioParameters_), radioNo(radioNo_) {};
    };

    struct RadioTxFrameMsg : public etl::message<18>
    {
        const Radio::TxPacket txPacket;
        uint8_t radioNo;
        RadioTxFrameMsg(const Radio::TxPacket &txPacket_, uint8_t radioNo_) : txPacket(txPacket_), radioNo(radioNo_) {}
    };

    struct RadioControlMsg : public etl::message<28>
    {
        const Radio::RadioParameters radioParameters;
        uint8_t radioNo;
        RadioControlMsg(const Radio::RadioParameters &radioParameters_, uint8_t radioNo_) : radioParameters(radioParameters_), radioNo(radioNo_) {};
    };

    struct ConfigUpdatedMsg : public etl::message<20> /* Don't change from 20!!!! They are used in MessageRouter*/
    {
        const Configuration &config;
        const OpenAce::Modulename moduleName;
        ConfigUpdatedMsg(const Configuration &config_, const OpenAce::Modulename &moduleName_) : config(config_), moduleName(moduleName_) {};
        ConfigUpdatedMsg(const Configuration &config_, const etl::string_view &moduleName_) : config(config_), moduleName(moduleName_) {};
    };

    /**
     * Send to inform receivers of the current connected clients over TCP.
     */
    struct AccessPointClientsMsg : public etl::message<21>
    {
        const etl::set<uint32_t, OPENACE_MAXIMUM_TCP_CLIENTS> msg;
        AccessPointClientsMsg(const etl::set<uint32_t, OPENACE_MAXIMUM_TCP_CLIENTS> &msg_) : msg(msg_) {};
        AccessPointClientsMsg() {};
    };

    /**
     * Send to inform receivers of the current connected clients over TCP.
     */
    struct GdlMsg : public etl::message<22>
    {
        GDLData msg;
        GdlMsg(GDLData msg_) : msg(msg_) {};
        GdlMsg() {};
    };

    /**
     * Message send when WIFI connection state changes
     */
    struct WifiConnectionStateMsg : public etl::message<24>
    {
        bool connected;
        uint32_t networkAddress;
        WifiConnectionStateMsg(bool connected_) : connected(connected_), networkAddress(0) {};
        WifiConnectionStateMsg(bool connected_, uint32_t networkAddress_) : connected(connected_), networkAddress(networkAddress_) {};
    };

    /**
     * Idle Message send at intervals that allows to due small tasks without creating a new task
     * Modules using this message should never block a task
     */
    struct IdleMsg : public etl::message<25>
    {
    };

    /**
     * Message send of the current adaptive radius size.
     */
    struct AdapativeRadiusMsg : public etl::message<26>
    {
        uint32_t radius;
        AdapativeRadiusMsg(uint32_t radius_) : radius(radius_) {};
    };
}