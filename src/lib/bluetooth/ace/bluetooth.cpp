#include <stdio.h>

#include "Bluetooth.hpp"
#include "ace/semaphoreguard.hpp"

OpenAce::PostConstruct Bluetooth::postConstruct()
{
    return OpenAce::PostConstruct::OK;
}

void Bluetooth::start()
{
    getBus().subscribe(*this);
};

void Bluetooth::stop()
{
    getBus().unsubscribe(*this);
};

void Bluetooth::getData(etl::string_stream &stream, const etl::string_view path) const
{
    (void)path;
    stream << "{";
    stream << "\"foo\":" << 0;
    stream << ",\"bar\":" << 1;
    stream << "}\n";
}

/**
 * Receive dataport messages and send it to all clients
 * TODO: CHange it such that each connected client can receive the OpenAce::DataPortMsg &msg
 */
void Bluetooth::on_receive(const OpenAce::DataPortMsg &msg)
{
    (void)msg;
}

void Bluetooth::on_receive_unknown(const etl::imessage &msg)
{
    (void)msg;
}

/**
 * Starts and stop LWiP when wifi connects or disconnects to cleanup any resources
 */
void Bluetooth::on_receive(const OpenAce::IdleMsg &msg)
{
    (void)msg;

}
