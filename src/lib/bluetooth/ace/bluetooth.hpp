#pragma once

#include <stdint.h>

/* FreeRTOS */
#include "FreeRTOS.h"
#include "task.h"

/* ETLCPP */
#include "etl/message_bus.h"
#include "etl/list.h"
#include "etl/string.h"

/* OpenAce */
#include "ace/constants.hpp"
#include "ace/messagerouter.hpp"
#include "ace/basemodule.hpp"
#include "ace/messages.hpp"
#include "ace/circularbuffer.hpp"

/**
 * AirCOnnect protocll for EFB's that only support AirCOnnect.
 * THis protocol is currently not recommende if you can also use GDL90
 */
class Bluetooth : public BaseModule, public etl::message_router<Bluetooth, OpenAce::DataPortMsg, OpenAce::IdleMsg>
{
    friend class message_router;
    struct
    {
        uint32_t toManyClients = 0;
        uint32_t newClientConnection = 0;
    } statistics;

private:
    virtual OpenAce::PostConstruct postConstruct() override;

    virtual void start() override;

    virtual void stop() override;

    void on_receive(const OpenAce::DataPortMsg &msg);
    
    void on_receive(const OpenAce::IdleMsg &msg);

    void on_receive_unknown(const etl::imessage &msg);

    virtual void getData(etl::string_stream &stream, const etl::string_view path) const override;

    // static void airConnectTask(void *arg);

public:
    static constexpr const char *NAME = "Bluetooth";
    Bluetooth(etl::imessage_bus &bus, const Configuration &config) : BaseModule(bus, NAME)
    {
        (void)config;
    }

    virtual ~Bluetooth() = default;
};
