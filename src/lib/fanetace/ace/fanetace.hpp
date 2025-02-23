#pragma once

#include <stdint.h>

#include "ace/constants.hpp"
#include "ace/messagerouter.hpp"
#include "ace/basemodule.hpp"
#include "ace/messages.hpp"

#include "etl/map.h"
#include "etl/message_bus.h"
#include "etl/queue_spsc_atomic.h"

class FanetAce : public BaseModule, public etl::message_router<FanetAce, OpenAce::RadioTxPositionRequestMsg, OpenAce::RadioRxLoraMsg>
{
    static constexpr uint8_t QUEUE_SIZE = 6;

private:
    friend class message_router;
    enum TaskState : uint32_t
    {
        EXIT = 1 << 0,
        NEW = 1 << 2,
    };
    struct
    {
        uint32_t totalReceived = 0;
        uint32_t queueFullErr = 0;
    } statistics;

    void on_receive_unknown(const etl::imessage &msg)
    {
        (void)msg;
    }

    static void FanetAceTask(void *arg);

    void on_receive(const OpenAce::RadioTxPositionRequestMsg &msg);
    void on_receive(const OpenAce::RadioRxLoraMsg &msg);

    TaskHandle_t taskHandle;
    etl::queue_spsc_atomic<OpenAce::ADSBString, QUEUE_SIZE, etl::memory_model::MEMORY_MODEL_SMALL> queue;

public:
    static constexpr const etl::string_view NAME = "Fanet";

    FanetAce(etl::imessage_bus &bus, const Configuration &config) : BaseModule(bus, NAME), taskHandle(nullptr)
    {
        (void)config;
    }

    virtual ~FanetAce() = default;

    virtual OpenAce::PostConstruct postConstruct() override;

    virtual void start() override;

    virtual void stop() override;

    virtual void getData(etl::string_stream &stream, const etl::string_view path) const override;
};
