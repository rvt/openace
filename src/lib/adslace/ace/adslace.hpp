#pragma once

/* System. */
#include <stdint.h>

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
#include "ace/basemodule.hpp"
#include "ace/messages.hpp"
#include "ace/coreutils.hpp"
#include "ace/basemodule.hpp"
#include "ace/datasourcetimestatstable.hpp"

#include "adsl/adsl.hpp"

/* Utils. */
// #include "ace/ldpc.hpp"
// #include "adsl_packet.hpp"

class ADSLAce : public BaseModule, ADSL::Connector, public etl::message_router<ADSLAce, GATAS::RadioRxGfskMsg, GATAS::OwnshipPositionMsg, GATAS::RadioTxPositionRequestMsg, GATAS::GpsStatsMsg>
{
    static constexpr int DEFAULT_IGNORE_DISTANCE = 25000;
    static constexpr int MAX_IGNORE_DISTANCE = 50000;

    friend class message_router;
    mutable struct
    {
        uint32_t receivedAircraftPositions = 0;
        uint32_t transmittedAircraftPositions = 0;
        uint32_t fecErr = 0;
        uint32_t outOfDistance = 0;
        uint32_t encrypted = 0;
        uint32_t protocolVersionErr = 0;
        uint32_t decryptionKeyErr = 0;
        uint32_t payloadUnSupportedErr = 0;
        uint32_t unknownErr = 0;
        uint32_t unsupportedFec = 0;
    } statistics;

    ADSL::Protocol protocol;
    GATAS::OwnshipPositionInfo ownshipPosition;

    GATAS::DataSourceTimeStatsTable<2> datasourceTimeStats;

    GATAS::GpsStatsMsg gpsStats;
    uint16_t distanceIgnore;

    struct Tx_Struct
    {
        Radio::RadioParameters radioParameters;
        uint8_t radioNo;
    };
    Tx_Struct rqMBandRadioParameters;
    Tx_Struct rqOBandRadioParameters;

public:
    static constexpr const etl::string_view NAME = "ADSL";
    ADSLAce(etl::imessage_bus &bus, const Configuration &config) : BaseModule(bus, NAME),
                                                                   protocol(this),
                                                                   ownshipPosition{}
    {
        auto di = config.valueByPath(DEFAULT_IGNORE_DISTANCE, "ADSL", "distanceIgnore");
        distanceIgnore = etl::max(0, etl::min(di, MAX_IGNORE_DISTANCE));
        protocol.init();
        protocol.addPayloadLength = true;
    }

    virtual ~ADSLAce() = default;

    virtual GATAS::PostConstruct postConstruct() override;
    virtual void start() override;
    virtual void getData(etl::string_stream &stream, const etl::string_view path) const override;

private:
    /**
     * Send a FreeRTOS message when a ADSL is received
     * This will release the sender from the task and allow it to continue in a seperate thread
     */
    void on_receive(const GATAS::RadioRxGfskMsg &msg);
    void on_receive(const GATAS::OwnshipPositionMsg &msg);
    void on_receive(const GATAS::RadioTxPositionRequestMsg &msg);
    void on_receive(const GATAS::GpsStatsMsg &msg);
    void on_receive(const GATAS::ConfigUpdatedMsg &msg);
    void on_receive_unknown(const etl::imessage &msg)
    {
        (void)msg;
    }

    // Transform a FLARM addressType to an gaTas address type
    GATAS::AddressType addressMapToAddressType(uint8_t addressMap) const;
    static uint8_t addressTypeToAddressMap(GATAS::AddressType addressType);
    static GATAS::AircraftCategory mapAircraftCategory(ADSL::TrafficPayload::AircraftCategory category);
    static ADSL::TrafficPayload::AircraftCategory mapAircraftCategory(GATAS::AircraftCategory category);

    //    int8_t parseFrame(const ADSL_Packet &packet, int16_t rssiDbm);

    virtual uint32_t adsl_getTick() const
    {
        return CoreUtils::timeUs32();
    }

    virtual bool adsl_sendFrame(const void *ctx, etl::span<const uint8_t> data);

    // Called when a payload/header/status is received by the protocol layer
    virtual void adsl_receivedTraffic(const ADSL::Header &header, const ADSL::TrafficPayload &tp);
    virtual void adsl_receivedStatus(const ADSL::Header &header, const ADSL::StatusPayload &tp);

    virtual void adsl_buildTraffic(const void *ctx, ADSL::TrafficPayload &tp);
    virtual void adsl_buildStatusPayload(const void *ctx, ADSL::StatusPayload &sp);

    ADSL::TrafficPayload::HorizontalPositionAccuracy mapHFOM(float hfomMeters);
    ADSL::TrafficPayload::NavigationIntegrity mapHPL(float hplMeters);
    ADSL::TrafficPayload::VerticalPositionAccuracy mapVFOM(float vfomMeters);
    ADSL::TrafficPayload::VelocityAccuracy mapVelAcc(float accVelMps);
};
