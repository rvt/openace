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

class FanetAce : public BaseModule, public FANET::Connector, public etl::message_router<FanetAce, OpenAce::RadioTxPositionRequestMsg, OpenAce::RadioRxLoraMsg, OpenAce::OwnshipPositionMsg, OpenAce::ConfigUpdatedMsg>
{
    static constexpr uint8_t QUEUE_SIZE = 6;

private:
    friend class message_router;
    static constexpr int32_t DEFAULT_IGNORE_DISTANCE = 25000;

    enum TaskState : uint32_t
    {
        EXIT = 1 << 0,
        HANDLETX = 1 << 2,
    };
    struct
    {
        uint32_t send = 0;
        uint32_t received = 0;
        uint32_t queueFullErr = 0;
        uint32_t outOfDistance = 0;
    } statistics;

    void on_receive_unknown(const etl::imessage &msg)
    {
        (void)msg;
    }

    static void FanetAceTask(void *arg);

    void on_receive(const OpenAce::RadioTxPositionRequestMsg &msg);
    void on_receive(const OpenAce::RadioRxLoraMsg &msg);
    void on_receive(const OpenAce::OwnshipPositionMsg &msg);
    void on_receive(const OpenAce::ConfigUpdatedMsg &msg);

    virtual uint32_t fanet_getTick() const override;
    virtual bool fanet_sendFrame(uint8_t codingRate, etl::span<const uint8_t> data) override;
    virtual void fanet_ackReceived(uint16_t id) override;

    TaskHandle_t taskHandle;
    FANET::Protocol protocol;
    SemaphoreHandle_t mutex;

    uint8_t radioNo;
    uint16_t distanceIgnore;

    OpenAce::OwnshipPositionInfo ownshipPosition;
    OpenAce::Config::OpenAceConfiguration openAceConfiguration;
    Radio::RadioParameters radioParameters;

    OpenAce::AircraftCategory mapAircraftCategory(FANET::TrackingPayload::AircraftType category) const;
    FANET::TrackingPayload::AircraftType mapAircraftCategory(OpenAce::AircraftCategory category) const;

public:
    static constexpr const etl::string_view NAME = "Fanet";

    FanetAce(etl::imessage_bus &bus, const Configuration &config) : BaseModule(bus, NAME), taskHandle(nullptr), protocol(this), mutex(nullptr), radioNo(0), distanceIgnore(DEFAULT_IGNORE_DISTANCE), openAceConfiguration(config.openAceConfig())
    {
        (void)config;
        protocol.ownAddress(FANET::Address{openAceConfiguration.address});

        int32_t di = config.valueByPath(25000, "Fanet", "distanceIgnore");
        distanceIgnore = etl::max((int32_t)0, etl::min(di, DEFAULT_IGNORE_DISTANCE));
    }

    virtual ~FanetAce() = default;

    virtual OpenAce::PostConstruct postConstruct() override;

    virtual void start() override;

    virtual void stop() override;

    virtual void getData(etl::string_stream &stream, const etl::string_view path) const override;
};
