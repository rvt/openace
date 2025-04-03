#pragma once

#include <stdint.h>

#include "ace/constants.hpp"
#include "ace/models.hpp"
#include "ace/messagerouter.hpp"
#include "ace/basemodule.hpp"
#include "ace/messages.hpp"
#include "ace/coreutils.hpp"

#include "adsbdatacollector.hpp"
#include "addresscache.hpp"

#include "mode-s.hpp"

#include "etl/map.h"
#include "etl/set.h"
#include "etl/message_bus.h"


#include "FreeRTOS.h"
#include "semphr.h"

/**
 * ADSBDecoder decodes ADSB message from a ADSB device and forwards that as NMEA Strings
 */
class ADSBDecoder : public BinaryReceiver, etl::message_router<ADSBDecoder, OpenAce::ADSBMessageBin, OpenAce::OwnshipPositionMsg, OpenAce::ConfigUpdatedMsg, OpenAce::IdleMsg, OpenAce::AdapativeRadiusMsg>
{
private:
    static constexpr uint8_t MAX_PLANES_TRACKED = 64;
    static constexpr uint8_t MAX_ADDRESS_CACHE_SIZE = 160;  // Address cache size for ignore planes that are to for/to high etc..
    static constexpr uint32_t ADSBDECODER_US_DELAY_SERIAL_AND_OVERHEAD = 20'000;

    friend class message_router;

private:
    struct
    {
        uint32_t crcErrors = 0;
        uint32_t knownAircraftFull = 0;
        uint32_t ignoredAircraftFull = 0;
        uint32_t totalMsgReceived = 0;
        uint32_t totalMsgIgnored = 0;
    } statistics;


    AddressCache<MAX_ADDRESS_CACHE_SIZE, 15'000'000> ignoredAirplanes; // A quick cache to store all airplanes that we already know we should not track for up to 30 seconds
    AdsbDataCollector<MAX_PLANES_TRACKED, 5'000'000> adsbDataCollector;

    int32_t filterAbove; // Filter out all aircraft above me in meters. 1000 means all aircraft 1000m or more above me will not get processed
    int32_t filterBelow; // Filter out all aircraft below me in meters. 100 means all aircraft 100m below me or more are not processed
    mode_s_t state;      
    SemaphoreHandle_t mutex;
    OpenAce::OwnshipPositionInfo ownshipPosition;
    uint32_t filterRadius=100'000;
    
public:
    static constexpr const etl::string_view NAME = "ADSBDecoder";
    ADSBDecoder(etl::imessage_bus &bus, const Configuration &config) : BinaryReceiver(bus, NAME)
    {
        filterAbove = config.valueByPath(true, NAME, "filterAbove");
        filterBelow = config.valueByPath(true, NAME, "filterBelow");
    }

    virtual ~ADSBDecoder()
    {
    }

    virtual OpenAce::PostConstruct postConstruct() override;

    virtual void start() override;

    virtual void stop() override;

    virtual void getData(etl::string_stream &stream, const etl::string_view path) const override;

private:
    void getConfiguration(const Configuration &config);

    void on_receive(const OpenAce::OwnshipPositionMsg &msg);

    void on_receive(const OpenAce::ConfigUpdatedMsg &msg);

    void on_receive(const OpenAce::IdleMsg &msg);

    void on_receive(const OpenAce::AdapativeRadiusMsg &msg);

    /**
     * Change a hex string into a byte array
     */
    void on_receive(const OpenAce::ADSBMessageBin &msg);

    virtual void receiveBinary(const uint8_t* data, uint8_t length) override;
    void processAdsbData(const uint8_t* data, uint8_t length);

    bool outOfAltitudeRange(int32_t otherCraftAltitude)
    {
        return (otherCraftAltitude - ownshipPosition.altitudeWgs84) > filterAbove || (ownshipPosition.altitudeWgs84 - otherCraftAltitude) > filterBelow;
    }

    void on_receive_unknown(const etl::imessage &msg)
    {
        (void)msg;
    }

    
};
