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

/* GATAS. */
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


class Ogn1 : public BaseModule, public etl::message_router<Ogn1, GATAS::RadioRxGfskMsg, GATAS::OwnshipPositionMsg,
    GATAS::RadioTxPositionRequestMsg, GATAS::BarometricPressureMsg, GATAS::GpsStatsMsg>
{
public:
    static constexpr uint8_t OGN_PACKET_LENGTH = 20;
    static constexpr uint8_t OGN_PACKET_LENGTH_FEC = 26;
private:
    static constexpr int DEFAULT_IGNORE_DISTANCE = 25000;
    static constexpr int MAX_IGNORE_DISTANCE = 50000;
    friend class message_router;
    struct
    {
        uint32_t receivedAircraftPositions = 0;
        uint32_t transmittedAircraftPositions = 0;
        uint32_t fecErr = 0;
        uint32_t outOfDistance = 0;
        uint32_t encrypted = 0;
        uint32_t nonPositional = 0;
        uint32_t relay[4] = {};
    } statistics;

    struct DataSourceTimeStats
    {
        etl::bitset<100> timeTenthMs;
        uint32_t frequency;
    };
    etl::vector<DataSourceTimeStats, 2> dataSourceTimeStats; // Two frequencies (Europe)

    GATAS::OwnshipPositionInfo ownshipPosition;
    int spinLock;
    GATAS::BarometricPressureMsg lastBarometricPressureMsg;
    GATAS::GpsStatsMsg gpsStats;
    uint16_t distanceIgnore;
    static LDPC_Decoder<Ogn1::OGN_PACKET_LENGTH*8, 48> decoder; 

public:
    static constexpr const etl::string_view NAME = "Ogn1";
    Ogn1(etl::imessage_bus& bus, const Configuration &config) :
        BaseModule(bus, NAME),
        spinLock(0),
        lastBarometricPressureMsg(),
        gpsStats()
    {
        auto di = config.valueByPath(DEFAULT_IGNORE_DISTANCE, "Ogn1", "distanceIgnore");
        distanceIgnore = etl::max(0, etl::min(di, MAX_IGNORE_DISTANCE));
    }

    virtual ~Ogn1() = default;

    virtual GATAS::PostConstruct postConstruct() override;
    virtual void start() override;
    virtual void getData(etl::string_stream &stream, const etl::string_view path) const override;

private:
    void on_receive(const GATAS::RadioRxGfskMsg &msg);
    void on_receive(const GATAS::OwnshipPositionMsg &msg);
    void on_receive(const GATAS::BarometricPressureMsg &msg);
    void on_receive(const GATAS::GpsStatsMsg &msg);
    void on_receive(const GATAS::RadioTxPositionRequestMsg &msg);
    void on_receive_unknown(const etl::imessage& msg);
    void on_receive(const GATAS::ConfigUpdatedMsg &msg);

    // Transform a Ogn addressType to an gaTas address type
    GATAS::AddressType addressTypeFromOgn(uint8_t addressType) const;
    uint8_t addressTypeToOgn(GATAS::AddressType addressType) const;

    /**
     * Keep track of what timestamp (roughly) we receive Ogn frames
    */
    void addReceiveStat(uint32_t frequency);

    int8_t parseFrame(OGN1_Packet &packet, int16_t rssiDbm);

    static uint8_t ErrCount(const uint8_t *err, uint8_t length);
    static uint8_t ErrCount(const uint8_t *output,const uint8_t *data, const uint8_t *err, uint8_t length);
    static uint8_t errorCorrect(uint8_t *output, uint8_t *data, uint8_t *err, uint8_t iter=32);


};

