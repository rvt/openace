#pragma once

/* System. */
#include <stdint.h>
#include <algorithm>

/* Vendor. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "message_buffer.h"

/* PICO. */
#include "pico/stdlib.h"
#include "pico/binary_info.h"

/* Vendor. */
#include "etl/map.h"
#include "etl/message_bus.h"
#include "etl/string.h"
#include "etl/bitset.h"

/* OpenACE. */
#include "ace/constants.hpp"
#include "ace/messagerouter.hpp"
#include "ace/basemodule.hpp"
#include "ace/messages.hpp"
#include "ace/utils.hpp"
#include "ace/coreutils.hpp"
#include "ace/basemodule.hpp"

/* Utils. */
#include "ace/ldpc.hpp"

#include "ognpacket.hpp"

class Ogn1 : public BaseModule, public etl::message_router<Ogn1, OpenAce::RadioRxFrame, OpenAce::OwnshipPositionMsg,
    OpenAce::RadioTxPositionRequest, OpenAce::BarometricPressure, OpenAce::GpsStatsMsg, OpenAce::ConfigUpdatedMsg>
{
    static constexpr int32_t DEFAULT_IGNORE_DISTANCE = 25000;
    static constexpr int32_t MAX_IGNORE_DISTANCE = 50000;
    friend class message_router;
    static constexpr uint8_t OGN_PACKET_LENGTH = 20;
    static constexpr uint8_t OGN_PACKET_LENGTH_FEC = 26;
    struct
    {
        uint32_t receivedAircraftPositions = 0;
        uint32_t transmittedAircraftPositions = 0;
        uint32_t fecErr = 0;
        uint32_t outOfDistance = 0;
        uint32_t encrypted = 0;
        uint32_t queueFullErr = 0;
        uint32_t nonPositional = 0;
        uint32_t relay[4] = {};
    } statistics;

    struct DataSourceTimeStats
    {
        etl::bitset<100> timeTenthMs;
        uint32_t frequency;
    };
    etl::vector<DataSourceTimeStats, 2> dataSourceTimeStats;

    TaskHandle_t taskHandle;
    QueueHandle_t frameConsumerQueue;
    OpenAce::OwnshipPositionInfo ownshipPosition;
    OpenAce::BarometricPressure lastBarometricPressure;
    OpenAce::GpsStatsMsg gpsStats;
    OpenAce::Config::OpenAceConfiguration openAceConfiguration;
    uint16_t distanceIgnore;
    LDPC_Decoder<OGN_PACKET_LENGTH*8, 48> decoder;
public:
    static constexpr const etl::string_view NAME = "Ogn1";
    Ogn1(etl::imessage_bus& bus, const Configuration &config) :
        BaseModule(bus, NAME),
        taskHandle(nullptr),
        frameConsumerQueue(nullptr),
        ownshipPosition(),
        lastBarometricPressure(),
        gpsStats(),
        openAceConfiguration(config.openAceConfig())
    {
        int32_t v = config.valueByPath(25000, "Ogn1", "distanceIgnore");
        distanceIgnore = std::max((int32_t)0, etl::min(v, MAX_IGNORE_DISTANCE));
    }

    virtual ~Ogn1() = default;

    virtual OpenAce::PostConstruct postConstruct() override;
    virtual void start() override;
    virtual void stop() override;
    virtual void getData(etl::string_stream &stream, const etl::string_view path) const override;

private:
    /**
     * Send a FreeRTOS message when a OgnFrame is received
     * This will release the sender from the task and allow it to continue in a seperate thread
    */
    void on_receive(const OpenAce::RadioRxFrame &msg);
    void on_receive(const OpenAce::OwnshipPositionMsg &msg);
    void on_receive(const OpenAce::BarometricPressure &msg);
    void on_receive(const OpenAce::GpsStatsMsg &msg);
    void on_receive(const OpenAce::RadioTxPositionRequest &msg);
    void on_receive_unknown(const etl::imessage& msg);
    void on_receive(const OpenAce::ConfigUpdatedMsg &msg);

    // Transform a Ogn addressType to an openAce address type
    OpenAce::AddressType addressTypeFromOgn(uint8_t addressType) const;
    uint8_t addressTypeToOgn(OpenAce::AddressType addressType) const;

    /**
     * Keep track of what timestamp (roughly) we receive Ogn frames
    */
    void addReceiveStat(uint32_t frequency);

    int8_t parseFrame(OGN1_Packet &packet, int16_t rssiDbm);

    /**
     * Parse a Ogn frame and send it
     *
     * Based on https://github.com/creaktive/flare/blob/master/flarm_decode.c
    */
    static void ognReceiveTask(void *arg);


    uint8_t ErrCount(const uint8_t *err, uint8_t length) const;
    uint8_t ErrCount(const uint8_t *output,const uint8_t *data, const uint8_t *err, uint8_t length) const;
    uint8_t errorCorrect(uint8_t *output, uint8_t *data, uint8_t *err, uint8_t iter=32);


};

