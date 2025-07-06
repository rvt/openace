#pragma once

//#include "flarm_common.hpp"

/* System. */
#include <stdint.h>
#include <algorithm>

/* Vendor. */
#include "FreeRTOS.h"
#include "queue.h"
#include "message_buffer.h"

/* PICO. */
#include "pico/stdlib.h"
#include "pico/binary_info.h"

/* Vendor. */
#include "etl/message_bus.h"
#include "etl/string.h"

/* GATAS. */
#include "ace/constants.hpp"
#include "ace/messagerouter.hpp"
#include "ace/basemodule.hpp"
#include "ace/messages.hpp"
#include "ace/coreutils.hpp"



class Flarm2024 : public BaseModule, public etl::message_router<Flarm2024, GATAS::ConfigUpdatedMsg, 
GATAS::RadioRxGfskMsg, GATAS::OwnshipPositionMsg, GATAS::RadioTxPositionRequestMsg>
{
    friend class message_router;
    static constexpr int DEFAULT_IGNORE_DISTANCE = 25000;
    static constexpr int MAX_IGNORE_DISTANCE = 50000;
private:
    struct
    {
        uint32_t receivedAircraftPositions = 0;
        uint32_t transmittedAircraftPositions = 0;
        uint32_t crcErr = 0;
        uint32_t outOfDistance = 0;
        uint32_t messageTypeNot0x02 = 0;
    } statistics;

    struct DataSourceTimeStats
    {
        etl::bitset<100> timeTenthMs;
        uint32_t frequency;
    };
    etl::vector<DataSourceTimeStats, 2> dataSourceTimeStats; // Two frequencies (Europe)

    etl::atomic<GATAS::OwnshipPositionInfo> ownshipPosition;
    GATAS::Config::GaTasConfiguration gaTasConfiguration;
    float deltaCourse;
    uint16_t distanceIgnore;
public:
    static constexpr const etl::string_view NAME = "Flarm";
    Flarm2024(etl::imessage_bus& bus, const Configuration &config) :
        BaseModule(bus, NAME),
        ownshipPosition(),
        deltaCourse(0.f)
    {
        auto di = config.valueByPath(DEFAULT_IGNORE_DISTANCE, "Flarm", "distanceIgnore");
        distanceIgnore = etl::max(0, etl::min(di, MAX_IGNORE_DISTANCE));
        gaTasConfiguration = config.gaTasConfig();
    }

    virtual ~Flarm2024() = default;

    virtual GATAS::PostConstruct postConstruct() override;
    virtual void start() override;
    virtual void stop() override;
    virtual void getData(etl::string_stream &stream, const etl::string_view path) const override;
private:


    /**
     * Send a FreeRTOS message when a FlarmFrame is received
     * This will release the sender from the task and allow it to continue in a seperate thread
    */
    void on_receive(const GATAS::RadioRxGfskMsg &msg);
    void on_receive(const GATAS::OwnshipPositionMsg &msg);
    void on_receive(const GATAS::ConfigUpdatedMsg &msg);
    void on_receive(const GATAS::RadioTxPositionRequestMsg &msg);

    void on_receive_unknown(const etl::imessage& msg)
    {
        (void)msg;
    }

    // Transform a FLARM addressType to an gaTas address type
    GATAS::AddressType addressTypeFromFlarm(uint8_t addressType) const;

    uint8_t addressTypeToFlarm(GATAS::AddressType) const;

    /**
     * Keep track of what timestamp (roughly) we receive flarm frames
    */
    void addReceiveStat(uint32_t frequency);

    /**
     * Parse a flarm frame and send it
     *
     * Based on https://github.com/creaktive/flare/blob/master/flarm_decode.c
    */
    int8_t parseFrame(uint32_t *flarm_pkt, uint32_t epochSeconds, int16_t rssiDbm);

};


