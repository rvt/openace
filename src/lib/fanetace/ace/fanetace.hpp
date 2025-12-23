#pragma once

#include <stdint.h>

#include "ace/constants.hpp"
#include "ace/messagerouter.hpp"
#include "ace/basemodule.hpp"
#include "ace/coreutils.hpp"
#include "ace/messages.hpp"

#include "etl/map.h"
#include "etl/message_bus.h"
#include "etl/queue_spsc_atomic.h"

#include "fanet/fanet.hpp"

class FanetAce : public BaseModule, public FANET::Connector, public etl::message_router<FanetAce, GATAS::RadioTxPositionRequestMsg, GATAS::RadioRxLoraMsg, GATAS::OwnshipPositionMsg, GATAS::ConfigUpdatedMsg>
{
    static constexpr uint8_t QUEUE_SIZE = 6;

private:
    friend class message_router;
    static constexpr int DEFAULT_IGNORE_DISTANCE = 25000;
    static constexpr int MAX_IGNORE_DISTANCE = 50000;

    enum TaskState : uint32_t
    {
        EXIT = 1 << 0,
        HANDLETX = 1 << 2,
    };
    struct
    {
        uint32_t send = 0;
        uint32_t received = 0;
        uint32_t outOfDistance = 0;
    } statistics;

    void on_receive_unknown(const etl::imessage &msg)
    {
        (void)msg;
    }

    static void FanetAceTask(void *arg);

    void on_receive(const GATAS::RadioTxPositionRequestMsg &msg);
    void on_receive(const GATAS::RadioRxLoraMsg &msg);
    void on_receive(const GATAS::OwnshipPositionMsg &msg);
    void on_receive(const GATAS::ConfigUpdatedMsg &msg);

    virtual uint32_t fanet_getTick() const override;
    virtual bool fanet_sendFrame(uint8_t codingRate, etl::span<const uint8_t> data) override;
    virtual void fanet_ackReceived(uint16_t id) override;

    TaskHandle_t taskHandle;
    FANET::Protocol protocol;
    SemaphoreHandle_t mutex;

    uint8_t radioNo;
    uint16_t distanceIgnore;

    GATAS::OwnshipPositionInfo ownshipPosition;
    GATAS::Config::GaTasConfiguration gaTasConfiguration;
    Radio::RadioParameters radioParameters;

    GATAS::AircraftCategory mapAircraftCategory(FANET::TrackingPayload::AircraftType category) const;
    FANET::TrackingPayload::AircraftType mapAircraftCategory(GATAS::AircraftCategory category) const;

public:
    static constexpr const etl::string_view NAME = "Fanet";

    FanetAce(etl::imessage_bus &bus, const Configuration &config) : BaseModule(bus, NAME), taskHandle(nullptr), protocol(this), mutex(nullptr), radioNo(0), distanceIgnore(DEFAULT_IGNORE_DISTANCE), ownshipPosition{}, gaTasConfiguration(config.gaTasConfig())
    {
        protocol.ownAddress(FANET::Address{gaTasConfiguration.conspicuity.icaoAddress});
        auto di = config.valueByPath(DEFAULT_IGNORE_DISTANCE, "Fanet", "distanceIgnore");
        distanceIgnore = etl::max(0, etl::min(di, MAX_IGNORE_DISTANCE));
    }

    virtual ~FanetAce() = default;

    virtual GATAS::PostConstruct postConstruct() override;

    virtual void start() override;

    virtual void getData(etl::string_stream &stream, const etl::string_view path) const override;
};
