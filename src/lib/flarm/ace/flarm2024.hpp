#pragma once

//#include "flarm_common.hpp"

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
#include "ace/coreutils.hpp"



class Flarm2024 : public BaseModule, public etl::message_router<Flarm2024, OpenAce::ConfigUpdatedMsg, 
OpenAce::RadioRxFrame, OpenAce::OwnshipPositionMsg, OpenAce::RadioTxPositionRequest>
{
    friend class message_router;

    static constexpr int32_t DEFAULT_IGNORE_DISTANCE = 25000;

    static constexpr etl::array<uint32_t, 4> BTEA_KEYS = {0xa5f9b21c, 0xab3f9d12, 0xc6f34e34, 0xd72fa378};
    static constexpr uint32_t BTEA_DELTA = 0x9e3779b9;
    static constexpr uint32_t SCRAMBLE = 0x956f6c77;
    static constexpr uint8_t BTEA_N = 4;
    static constexpr uint8_t BTEA_ROUNDS = 6;
private:

#pragma pack(push, 4)
    struct RadioPacket
    {
        uint32_t aircraftID : 24;            // Bits 0-23: aircraft ID
        uint8_t messageType : 4;             // Bits 24-27: message type
        uint8_t addressType : 3;             // Bits 28-30: address (ID) type
        uint32_t reserved1 : 23;             // Bits 31-53: always 0
        bool stealth : 1;                    // Bit 54: stealth flag
        bool noTrack : 1;                    // Bit 55: no-track flag
        uint8_t reserved3 : 4;               // Bits 56-57: always 1
        uint8_t reserved4 : 4;               // Bits 58-59: always 0
        uint8_t reserved5 : 2;               // Bits 60-61: always 1
        uint8_t flarmTimestampLSB : 4;       // Bits 66-69: FLARM timestamp LSB
        uint8_t aircraftType : 4;            // Bits 70-73: aircraft type
        uint8_t reserved7 : 1;               // Bit 74: always 0
        uint16_t altitude : 13;              // Bits 75-87: altitude in meters + 1000, enscaled(12,1)
        uint32_t latitude : 20;              // Bits 88-107: latitude (rounded and with MS bits removed)
        uint32_t longitude : 20;             // Bits 108-127: longitude (rounded and with MS bits removed)
        uint16_t turnRate : 9;                // Bits 128-136: turn rate, degs/sec times 20, enscaled(6,2), signed
        uint16_t groundSpeed : 10;           // Bits 137-146: horizontal speed, m/s times 10, enscaled(8,2)
        uint16_t verticalSpeed : 9;           // Bits 147-155: vertical speed, m/s times 10, enscaled(6,2), signed
        uint16_t course : 10;                // Bits 156-165: course direction, degrees (0-360) times 2
        uint8_t movementStatus : 2;          // Bits 166-167: 2-bit integer for movement status
        uint8_t gnssHorizontalAccuracy : 6;  // Bits 168-173: GNSS horizontal accuracy, meters times 10, enscaled(3,3)
        uint8_t gnssVerticalAccuracy : 5;    // Bits 174-178: GNSS vertical accuracy, meters times 4, enscaled(2,3)
        uint8_t unknownData : 5;             // Bits 179-183: unknown data
        uint8_t reserved8 : 8;               // Bits 184-191: always 0

        uint16_t checksum;
        static constexpr uint8_t packetLength = 24; // Packet length
        static constexpr uint8_t totalLengthWCRC = packetLength + 2;  // Packet length with CRC
    } __attribute__((packed));
#pragma pack(pop)

    struct
    {
        uint32_t receivedAircraftPositions = 0;
        uint32_t transmittedAircraftPositions = 0;
        uint32_t crcErr = 0;
        uint32_t outOfDistance = 0;
        uint32_t queueFullErr = 0;
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
    OpenAce::Config::OpenAceConfiguration openAceConfiguration;
    float deltaCourse;
    uint16_t distanceIgnore;
public:
    static constexpr const etl::string_view NAME = "Flarm";
    Flarm2024(etl::imessage_bus& bus, const Configuration &config) :
        BaseModule(bus, NAME),
        taskHandle(nullptr),
        frameConsumerQueue(nullptr),
        ownshipPosition(),
        deltaCourse(0.f)
    {
        int32_t di = config.valueByPath(25000, "Flarm", "distanceIgnore");
        distanceIgnore = etl::max((int32_t)0, etl::min(di, DEFAULT_IGNORE_DISTANCE));
        openAceConfiguration = config.openAceConfig();
    }

    virtual ~Flarm2024() = default;

    virtual OpenAce::PostConstruct postConstruct() override;
    virtual void start() override;
    virtual void stop() override;
    virtual void getData(etl::string_stream &stream, const etl::string_view path) const override;
private:


    /**
     * Send a FreeRTOS message when a FlarmFrame is received
     * This will release the sender from the task and allow it to continue in a seperate thread
    */
    void on_receive(const OpenAce::RadioRxFrame &msg);
    void on_receive(const OpenAce::OwnshipPositionMsg &msg);
    void on_receive(const OpenAce::ConfigUpdatedMsg &msg);
    void on_receive(const OpenAce::RadioTxPositionRequest &msg);

    void on_receive_unknown(const etl::imessage& msg)
    {
        (void)msg;
    }

    // Transform a FLARM addressType to an openAce address type
    OpenAce::AddressType addressTypeFromFlarm(uint8_t addressType) const;

    uint8_t addressTypeToFlarm(OpenAce::AddressType) const;

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

    void bteaDecode(uint32_t *data) const;
    void bteaEncode(uint32_t *data) const;
    void scramble(uint32_t *data, uint32_t timestamp) const;

};


