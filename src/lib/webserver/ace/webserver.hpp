#pragma once

#include <stdint.h>

#include "ace/constants.hpp"
#include "ace/messagerouter.hpp"
#include "ace/basemodule.hpp"
#include "ace/messages.hpp"

#include "etl/map.h"
#include "etl/message_bus.h"

class Webserver : public BaseModule, public etl::message_router<Webserver, OpenAce::WifiConnectionState>
{
    friend class message_router;
public:
    static constexpr const etl::string_view NAME = "Webserver";
    Webserver(etl::imessage_bus& bus, const Configuration &config) : BaseModule(bus, NAME)
    {
        (void)config;
    }

    virtual ~Webserver() = default;

    virtual void getData(etl::string_stream &stream, const etl::string_view path) const override;

    virtual OpenAce::PostConstruct postConstruct() override;

    virtual void start() override;

    virtual void stop() override;

    void on_receive(const OpenAce::WifiConnectionState &wcs);

    void on_receive_unknown(const etl::imessage& msg);
};
