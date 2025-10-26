#include <stdio.h>

#include "../airconnect.hpp"
#include "ace/lwiplock.hpp"

GATAS::PostConstruct AirConnect::postConstruct()
{
    mutex = xSemaphoreCreateMutex();
    if (mutex == nullptr)
    {
        return GATAS::PostConstruct::MUTEX_ERROR;
    }
    return GATAS::PostConstruct::OK;
}

void AirConnect::start()
{
    getBus().subscribe(*this);
};

void AirConnect::getData(etl::string_stream &stream, const etl::string_view path) const
{
    (void)path;
    stream << "{";
    stream << "\"connectedClients\":" << connectedClients.size();
    stream << ",\"toManyClients\":" << statistics.toManyClients;
    stream << ",\"newClientConnection\":" << statistics.newClientConnection;
    stream << "}\n";
}

/**
 * Receive dataport messages and send it to all clients
 */
void AirConnect::on_receive(const GATAS::DataPortMsg &msg)
{
    (void)msg;
    auto guard = SemaphoreGuard<true>(10, mutex);
    for (auto &connection : connectedClients)
    {
        connection.write(msg.sentence);
    }
}

void AirConnect::on_receive_unknown(const etl::imessage &msg)
{
    (void)msg;
}

void AirConnect::on_receive(const GATAS::Every5SecMsg &msg)
{
    (void)msg;
    auto guard = SemaphoreGuard<true>(5, mutex);

    for (auto it = connectedClients.begin(); it != connectedClients.end();)
    {
        if (it->isDisconnected())
        {
            it = connectedClients.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void AirConnect::tcpAcceptHandler(struct tcp_pcb *pcb)
{
    if (connectedClients.full())
    {
        statistics.toManyClients++;
    } else {
        auto guard = SemaphoreGuard<true>(1000, mutex);
        auto &client = connectedClients.emplace_back(pcb, this);
        (void)client;
        client.write("AOK");
        statistics.newClientConnection++;
    }
}

/**
 * Track wifi connects and disconnect
 */
void AirConnect::on_receive(const GATAS::WifiConnectionStateMsg &wcs)
{
    wifiConnected = wcs.wifiMode != GATAS::WifiMode::NC;

    if (wifiConnected)
    {
        tcpListener.start();
    }

    if (!wifiConnected)
    {
        tcpListener.stop();
    }
}
