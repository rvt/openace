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

#include "etl/math.h"

/**
 * This decoder requires that both GGA and GMC sentences are received from the GPS and that each sentences us correct ms resolution
 * When both coralated sentences are sned. It will send out a ownship position message
 */
class GpsDecoder : public BaseModule, public etl::message_router<GpsDecoder, GATAS::GPSSentenceMsg>
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

    float velocityNorth = 0;
    float velocityEast = 0;
    RatePerSecond altitudeGeoid{GATAS_EMAFLOAT_K_FACTOR_2PS, 2}; // Field 9
    float geoidSeparation = 0;                                     // Field 11
    float groundSpeed = 0;
    RatePerSecond course{GATAS_EMAFLOAT_K_FACTOR_2PS, 2};
    float latitude = 0;
    float longitude = 0;

    // 0: Fix not valid
    // 1: GPS fix
    // 2: Differential GPS fix (DGNSS), SBAS, OmniSTAR VBS, Beacon, RTX in GVBS mode
    // 3: Not applicable
    // 4: RTK Fixed, xFill
    // 5: RTK Float, OmniSTAR XP/HP, Location RTK, RTX
    // 6: INS Dead reckoning
    uint8_t fixQuality = 0;
    uint8_t satellitesTracked = 0;
    float pDop = 255;

    minmea_time lastRMCTimestamp;
    minmea_time lastGGATimestamp;
    const uint32_t taskStartTime;
    uint8_t fixType = 0;

private:
    void on_receive(const GATAS::GPSSentenceMsg &msg);

    /**
     * Convert an minmea_float with altitude/height information in meters
     */
    float convertToMeters(const minmea_float &value, char unit, float defaultValue) const;

    /**
     * Send message when both GGA and RMC sentences are received
     */
    void sendMessageWhenGGAisRMC();

    void on_receive_unknown(const etl::imessage &msg)
    {
        (void)msg;
    }

    uint8_t getGpsRate() const
    {
        return (((float)statistics.receivedRMC) / (CoreUtils::timeS32() - taskStartTime)) + 0.5f;
    }

    float getFloat(const minmea_float &f, float defaultValue)
    {
        if (f.scale == 0)
        {
            return defaultValue;
        }
        return minmea_tofloat(&f);
    }

public:
    static constexpr const etl::string_view NAME = "GpsDecoder";
    GpsDecoder(etl::imessage_bus &bus, const Configuration &config) : BaseModule(bus, NAME),
                                                                      lastRMCTimestamp({0, 0, 0, 0}),
                                                                      lastGGATimestamp({0, 0, 0, 0}),
                                                                      taskStartTime(CoreUtils::timeS32())
    {
        (void)config;
    }

    virtual ~GpsDecoder() = default;

    virtual GATAS::PostConstruct postConstruct() override;

    virtual void start() override;
    virtual void stop() override;

    virtual void getData(etl::string_stream &stream, const etl::string_view path) const override;
};
