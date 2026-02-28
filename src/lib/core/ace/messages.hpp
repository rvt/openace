#pragma once

#include "constants.hpp"
#include "basemodule.hpp"
#include "models.hpp"
#include "poolallocator.hpp"
#include "etl/message.h"
#include "etl/message_router.h"
#include "etl/message_bus.h"
#include "etl/string.h"
#include "etl/set.h"
#include "etl/array.h"

namespace GATAS
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

    struct AircraftPositionsMsg : public etl::message<33>
    {
        const etl::vector<AircraftPositionInfo, 8> positions;
        int16_t rssidBm; // Received signal strength indicator in dB
        AircraftPositionsMsg(const etl::vector<AircraftPositionInfo, 8> &positions_, int16_t rssidBm_) : positions(positions_), rssidBm(rssidBm_) {}
        AircraftPositionsMsg(const etl::vector<AircraftPositionInfo, 8> &positions_) : positions(positions_), rssidBm(INT16_MIN) {}
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
    };

    struct GpsStatsMsg : public etl::message<14>
    {
        GATAS::GpsFix gpsFix;       // gpsFix, calculated from GPS
        uint8_t satsUsedForFix;     // From GGA Sentence
        float pDop;                 // From GSA Sentence
        float hDop;                 // From GSA Sentence
        float vDop;                 // From GSA Sentence
        pDopInterpretation pDopInt; // Interpretation from pDop
        GpsStatsMsg() : gpsFix(), satsUsedForFix(0), pDop(0.0f), hDop(0.0f), vDop(0.f), pDopInt(pDopInterpretation::POOR) {};
        GpsStatsMsg(GATAS::GpsFix gpsFix_, uint8_t satsUsedForFix_, float pDop_, float hDop_, float vDop_, pDopInterpretation pDopInt_) : gpsFix(gpsFix_), satsUsedForFix(satsUsedForFix_), pDop(pDop_), hDop(hDop_), vDop(vDop_), pDopInt(pDopInt_) {};
    };

    struct BarometricPressureMsg : public etl::message<15>
    {
        float pressurehPa;    // Preasure in hPa (hectopascal)
        uint32_t usSinceBoot; // Time since boot
        BarometricPressureMsg(float pressurehPa_, uint32_t usSinceBoot_) : pressurehPa(pressurehPa_), usSinceBoot(usSinceBoot_) {};
        BarometricPressureMsg() : pressurehPa(0), usSinceBoot(0) {};
    };

    struct RadioRxMsgBase
    {
        mutable uint8_t *frame; // Mutable Hack to set the ptr to nullptr once it was freed
        mutable size_t lengthBytes; // hack to allow reset the length on a const object from the messagebus
        uint32_t epochSeconds;
        uint32_t frequency;
        GATAS::DataSource dataSource;
        int8_t rssidBm;

        RadioRxMsgBase(uint8_t *frame_, size_t lengthBytes_, uint32_t epochSeconds_, uint32_t frequency_, GATAS::DataSource dataSource_, int8_t rssidBm_)
            : frame(frame_), lengthBytes(lengthBytes_), epochSeconds(epochSeconds_), frequency(frequency_), dataSource(dataSource_), rssidBm(rssidBm_) {}

        uint32_t *frame32() const
        {
            return reinterpret_cast<uint32_t *>(frame);
        }

        etl::span<uint32_t> frame32Span() const
        {
            return etl::span<uint32_t>(reinterpret_cast<uint32_t *>(frame), (lengthBytes + 3) / 4);
        }

        etl::span<uint8_t> frameSpan() const
        {
            return etl::span<uint8_t>(frame, lengthBytes);
        }
    };

    struct RadioRxMsg : public RadioRxMsgBase, public etl::message<27>
    {
        explicit RadioRxMsg(uint8_t *data_, size_t length_, uint32_t epochSeconds_, uint32_t frequency_, GATAS::DataSource dataSource_, int8_t rssidBm_)
            : RadioRxMsgBase(data_, length_, epochSeconds_, frequency_, dataSource_, rssidBm_)
        {
        }
    };

    struct RadioRxManchesterMsg : public RadioRxMsgBase, public etl::message<16>
    {
        mutable uint8_t *error; // Mutable Hack to set the ptr to nullptr once it was freed

        explicit RadioRxManchesterMsg(uint8_t *data_, uint8_t *error_, size_t length_, uint32_t epochSeconds_, uint32_t frequency_, GATAS::DataSource dataSource_, int8_t rssidBm_)
            : RadioRxMsgBase(data_, length_, epochSeconds_, frequency_, dataSource_, rssidBm_), error(error_) {}

        uint32_t *err32()
        {
            return reinterpret_cast<uint32_t *>(error);
        }

        etl::span<uint32_t> error32Span() const
        {
            return etl::span<uint32_t>(reinterpret_cast<uint32_t *>(error), (lengthBytes + 3) / 4);
        }

        etl::span<uint8_t> errorSpan() const
        {
            return etl::span<uint8_t>(error, lengthBytes);
        }
    };

    /**
     * @brief Message to instruct a protocol to transmit the current data over the protocol
     *
     */
    struct RadioTxPositionRequestMsg : public etl::message<2>
    {
        const GATAS::RadioParameters radioParameters;
        uint8_t radioNo;
        RadioTxPositionRequestMsg(const GATAS::RadioParameters &radioParameters_, uint8_t radioNo_) : radioParameters(radioParameters_), radioNo(radioNo_) {};
    };

    /**
     * @brief Message send to transmit a frame over the radio
     *
     */
    struct RadioTxFrameMsg : public etl::message<18>
    {
        Radio::TxPacket txPacket;
        uint8_t radioNo;
        RadioTxFrameMsg(const Radio::TxPacket &txPacket_, uint8_t radioNo_) : txPacket(txPacket_), radioNo(radioNo_) {};
    };

    /**
     * @brief Message to control the radio to a new protocol, frequency etc
     *
     */
    struct RadioControlMsg : public etl::message<28>
    {
        const GATAS::RadioParameters radioParameters;
        uint8_t radioNo;
        RadioControlMsg(const GATAS::RadioParameters &radioParameters_, uint8_t radioNo_) : radioParameters(radioParameters_), radioNo(radioNo_) {};
    };

    struct ConfigUpdatedMsg : public etl::message<20> /* Don't change from 20!!!! They are used in MessageRouter*/
    {
        const Configuration &config;
        const GATAS::Modulename moduleName;
        ConfigUpdatedMsg(const Configuration &config_, const GATAS::Modulename &moduleName_) : config(config_), moduleName(moduleName_) {};
        ConfigUpdatedMsg(const Configuration &config_, const etl::string_view &moduleName_) : config(config_), moduleName(moduleName_) {};
    };

    /**
     * Send to inform receivers of the current connected clients over TCP.
     */
    struct AccessPointClientsMsg : public etl::message<21>
    {
        const etl::set<uint32_t, GATAS_MAXIMUM_TCP_CLIENTS> msg;
        AccessPointClientsMsg(const etl::set<uint32_t, GATAS_MAXIMUM_TCP_CLIENTS> &msg_) : msg(msg_) {};
        AccessPointClientsMsg() {};
    };

    /**
     * Send to inform receivers of the current connected clients over TCP.
     */
    struct GdlMsg : public etl::message<22>
    {
        GDLData msg;
    };

    /**
     * Message send when WIFI connection state changes
     * NOTE: Don't change message ID!
     */
    struct WifiConnectionStateMsg : public etl::message<24>
    {
        GATAS::WifiMode wifiMode;
        uint32_t gatasIp;
        uint32_t gateWay;
        WifiConnectionStateMsg(GATAS::WifiMode wifiMode_) : wifiMode(wifiMode_), gatasIp(0), gateWay(0) {};
        WifiConnectionStateMsg(GATAS::WifiMode wifiMode_, uint32_t gatasIp_, uint32_t gateWay_) : wifiMode(wifiMode_), gatasIp(gatasIp_), gateWay(gateWay_) {};
    };

    /**
     * Message send of the current adaptive radius size.
     */
    struct AdapativeRadiusMsg : public etl::message<26>
    {
        uint32_t radius;
        AdapativeRadiusMsg(uint32_t radius_) : radius(radius_) {};
    };

    /**
     * Idle Message send at intervals that allows to due small tasks without creating a new task
     * Modules using this message should never block a task
     */
    struct IdleMsg : public etl::message<25>
    {
    };
    struct Every1SecMsg : public etl::message<29>
    {
    };
    struct Every5SecMsg : public etl::message<30>
    {
    };
    struct Every15SecMsg : public etl::message<31>
    {
    };
    struct Every30SecMsg : public etl::message<32>
    {
    };
    struct Every300SecMsg : public etl::message<34>
    {
    };
}