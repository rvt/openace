#pragma once

#include "flarm_common.hpp"

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



struct flarmV7Packet_t
{
    /********************/
// 0
    uint32_t address:24;       // A 0
// 3
    uint8_t  zero0:4;          // 0 3
    uint8_t  addressType:4;    // a 3
// 4
    uint16_t verticalSpeed:10; // v 4 in dm/s
    uint8_t  turnRate:3;       // q 5 (plane on ground), 5 (no/slow turn), 4 (right turn >14deg), 7 (left turn >14deg)
    bool     stealth:1;        // z 5
    bool     noTrack:1;        // y 5
    uint8_t  parity:1;         // 5
// 6
    uint16_t vacc_m:6;          // g 6 // These might come from GPS?
    uint16_t hacc_m:6;          // h 6 // These might come from GPS?
    uint8_t aircraftType:4;    // t 4
    /********************/
// 8
    uint32_t latitude:19;      // L 8
    uint16_t altitude:13;      // a 10 in meters 0..8192 meters
    /********************/
// 12
    uint32_t longitude:20;     // N 12
    uint16_t zero1:10;         // 0 14
    uint8_t  speedscale:2;     // FF 15 0,1,2,3
    /********************/
// 16
    int8_t  ns[4];            // S s K k 16 extrapolated velocity vector components for 4s intervals, meters in 4 seconds
// 20
    int8_t  ew[4];            // E e P p 20
// 24
    uint16_t checksum;
// 26
    uint16_t pad;
    static constexpr uint8_t packetLength = 24; // Packet length
    static constexpr uint8_t totalLength = 26;  // Packet length with CRC
};
//__attribute__((packed))
;

//  0     AAAA AAAA    device address
//  1     AAAA AAAA
//  2     AAAA AAAA
//  3     00aa 0000    aa = 10 or 01

//  4     vvvv vvvv    vertical speed (dm/s)
//  5     0yzq qqvv    y=notrack z=priv, q=turn rate *)
//  6     hhgg gggg    min(62, gps vacc m)
//  7     tttt hhhh    plane type, min(62, gps hacc m)

//  8     LLLL LLLL    Latitude
//  9     LLLL LLLL
// 10     aaaa aLLL
// 11     aaaa aaaa    Altitude

// 12     NNNN NNNN    Longitude
// 13     NNNN NNNN
// 14     0000 NNNN
// 15     FF00 0000    multiplying factor

// 16     SSSS SSSS    vel_n[0] **)
// 17     ssss ssss    vel_n[1]
// 18     KKKK KKKK    vel_n[2]
// 19     kkkk kkkk    vel_n[3]

// 20     EEEE EEEE    vel_e[0]
// 21     eeee eeee    vel_e[0]
// 22     PPPP PPPP    vel_e[0]
// 23     pppp pppp    vel_e[0]

enum
{
    TURN_RATE_0,
    TURN_RATE_ON_GROUND, //1 (plane on ground)
    TURN_RATE_2,
    TURN_RATE_3,
    TURN_RATE_RIGHT_TURN, //4 (right turn >14deg)
    TURN_RATE_NO_TURN, //5 (no/slow turn)
    TURN_RATE_6,
    TURN_RATE_LEFT_TURN //7 (left turn >14deg)
};

void flarmMakekey(uint32_t key[4], uint32_t epochSeconds, uint32_t address);

void flarmEncrypt(uint32_t *flarm_pkt, uint32_t epochSeconds);
void flarmDecrypt(uint32_t *flarm_pkt, uint32_t epochSeconds);

uint16_t flarmCalculateChecksum(uint8_t* flarm_pkt, uint8_t length);





class Flarm_2023 : public BaseModule, public etl::message_router<Flarm_2023, OpenAce::RadioRxGfskMsg, OpenAce::OwnshipPositionMsg, OpenAce::RadioTxPositionRequestMsg>
{
    static constexpr int32_t DEFAULT_IGNORE_DISTANCE = 25000;
    static constexpr int32_t MAX_IGNORE_DISTANCE = 50000;
    friend class message_router;
    struct
    {
        uint32_t receivedAircraftPositions = 0;
        uint32_t transmittedAircraftPositions = 0;
        uint32_t crcErrors = 0;
        uint32_t outOfDistance = 0;
        uint32_t zero0x01Err = 0;
        uint32_t zero0Err = 0;
        uint32_t addressTypeErr = 0;
        uint32_t addressTypeFlarm = 0;
        uint32_t addressTypeRandom = 0;
        uint32_t addressTypeICAO = 0;
        uint32_t queueFull = 0;
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
    float deltaCourse;
    uint16_t distanceIgnore;
public:
    static constexpr const etl::string_view NAME = "Flarm_2023";
    Flarm_2023(etl::imessage_bus& bus, const Configuration &config) :
        BaseModule(bus, NAME),
        taskHandle(nullptr),
        frameConsumerQueue(nullptr),
        ownshipPosition(),
        deltaCourse(0.f)
    {
        int32_t v = config.valueByPath(25000, "Flarm_2023", "distanceIgnore");
        distanceIgnore = etl::max((int32_t)0, etl::min(v, MAX_IGNORE_DISTANCE));
    }

    virtual ~Flarm_2023() = default;

    virtual OpenAce::PostConstruct postConstruct() override;
    virtual void start() override;
    virtual void stop() override;
    virtual void getData(etl::string_stream &stream, const etl::string_view path) const override;
private:
    /**
     * Send a FreeRTOS message when a FlarmFrame is received
     * This will release the sender from the task and allow it to continue in a seperate thread
    */
    void on_receive(const OpenAce::RadioRxGfskMsg &msg)
    {
        if (msg.dataSource == OpenAce::DataSource::FLARM)
        {
            const OpenAce::RadioRxGfskMsg cpy = msg;
            if (xQueueSendToBack(frameConsumerQueue, &cpy, TASK_DELAY_MS(5)) != pdPASS )
            {
                statistics.queueFull++;
            }
        }
    }

    void on_receive(const OpenAce::OwnshipPositionMsg &msg)
    {
        ownshipPosition = msg.position;
    }

    /**
     * Handle a request to send a Flarm packet.
     * The packets is assembled and then send over the respected radio
     * Only reason to return the packet is to beable to test it.
    */
    const flarmV7Packet_t on_receive(const OpenAce::RadioTxPositionRequestMsg &msg);

    void on_receive(const OpenAce::ConfigUpdated &msg)
    {
    }

    void on_receive_unknown(const etl::imessage& msg)
    {
        (void)msg;
    }

    // Transform a FLARM addressType to an openAce address type
    OpenAce::AddressType addressType(uint8_t addressType);

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
    static void flarmReceiveTask(void *arg);


    uint8_t getSpeedScaling() const;
    uint8_t getSpeedScaling(float groundSpeed, float verticalSpeed) const;

    uint8_t getTurnState() const;

    /**
     * THis generates a vector of 4 bytes that represents the velocity vector
     * Each byte is a 4 second interval of the velocity vector as a uint8_t, but it's actually a two's complement signed int
     * 217 means 39 (256-217) = 39
     * So sends out a predicted path over the next 17 seconds in intervals of [2-5] [6-9] [10-13] []14-17]
    */
    void extrapolatVelocityVector(float deltaHeadingRad, float velNorthMS, float velEastMS, uint8_t speedScale, int8_t velN[4], int8_t velE[4]) const;

};


