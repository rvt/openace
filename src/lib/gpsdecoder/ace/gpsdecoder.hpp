#pragma once

#include <stdint.h>

#include "ace/constants.hpp"
#include "ace/models.hpp"

#include "ace/messagerouter.hpp"
#include "ace/basemodule.hpp"
#include "ace/messages.hpp"
#include "ace/coreutils.hpp"
#include "ace/EMA.hpp"

#include "minmea.h"



/**
 * This decoder requires that both GGA and GMC sentences are received from the GPS and that each sentences us correct ms resolution
 * When both coralated sentences are sned. It will send out a ownship position message
*/
class GpsDecoder : public BaseModule, public etl::message_router<GpsDecoder, OpenAce::GPSSentenceMsg>
{
    friend class message_router;
    struct
    {
        uint32_t receivedGGA = 0;
        uint32_t receivedRMC = 0;
        uint32_t receivedGSA = 0;
        uint32_t receivedOther = 0;
        uint32_t startTime = CoreUtils::timeS32();
    } statistics;

    static constexpr float INVALID_CONVERSION = -9999;

    float velocityNorth;
    float velocityEast;
    RatePerSecond<OPENACE_GPS_FREQUENCY> altitudeWgs84{OPENACE_EMAFLOAT_K_FACTOR_1S};
    RatePerSecond<OPENACE_GPS_FREQUENCY> heightGeoidWGS84{OPENACE_EMAFLOAT_K_FACTOR_1S};   // Height of geoid above WGS84 ellipsoid eg sea level
    RatePerSecond<OPENACE_GPS_FREQUENCY> groundSpeed{OPENACE_EMAFLOAT_K_FACTOR_1S};
    RatePerSecond<OPENACE_GPS_FREQUENCY> course{OPENACE_EMAFLOAT_K_FACTOR_1S};
    RatePerSecond<OPENACE_GPS_FREQUENCY> latitude{OPENACE_EMAFLOAT_K_FACTOR_1S};
    RatePerSecond<OPENACE_GPS_FREQUENCY> longitude{OPENACE_EMAFLOAT_K_FACTOR_1S};

    uint8_t fixQuality;
    uint8_t satellitesTracked;
    float pDop;

    minmea_time lastRMCTimestamp;
    minmea_time lastGGATimestamp;
private:
    void on_receive(const OpenAce::GPSSentenceMsg& msg);

    /**
     * Convert an minmea_float with altitude/height information in meters
    */
    float convertToMeters(const struct minmea_float *value, char unit) const;

    /**
     * Send message when both GGA and RMC sentences are received
    */
    void sendMessageWhenGGAisRMC();

    void on_receive_unknown(const etl::imessage& msg)
    {
        (void)msg;
    }
public:
    static constexpr const etl::string_view NAME = "GpsDecoder";
    GpsDecoder(etl::imessage_bus& bus, const Configuration &config) : BaseModule(bus, NAME),
        fixQuality(0),
        satellitesTracked(0),
        pDop(255),
        lastRMCTimestamp({0,0,0,0}),
        lastGGATimestamp({0,0,0,0})
    {
        (void)config;
    }

    virtual ~GpsDecoder() = default;

    virtual OpenAce::PostConstruct postConstruct() override;

    virtual void start() override;
    virtual void stop() override;

    virtual void getData(etl::string_stream &stream, const etl::string_view path) const override;
};
