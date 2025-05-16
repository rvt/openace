#pragma once

#include <stdint.h>
#include <sys/time.h>

/* PICO. */
#include "pico/time.h"

/* OpenACE. */
#include "ace/constants.hpp"
#include "ace/messagerouter.hpp"
#include "ace/basemodule.hpp"
#include "ace/messages.hpp"
#include "ace/circularbuffer.hpp"

/* LwIP */
#include "lwip/pbuf.h"
#include "lwip/udp.h"

/* ETL CPP */
#include "etl/message_bus.h"
#include "etl/set.h"
#include "etl/list.h"

/**
 * Client that can connect to a host and a port and expect to receive line terminated NMEA Messages
 * TODO: de-Couple UDP from the GDL90 so this service send GDL Message over the messagebus which can then be send over UDP or Serial or BlueToolh
 */
class GDLoverUDP : public BaseModule, public etl::message_router<GDLoverUDP, OpenAce::GdlMsg, OpenAce::AccessPointClientsMsg, OpenAce::ConfigUpdatedMsg, OpenAce::WifiConnectionStateMsg>
{
    static constexpr uint16_t GDL90OVERUDP_DEFAULT_PORT = 4000;                                   // Default port
    static constexpr uint8_t GDL90OVERUDP_MAX_PORTS = 4;                                          // Maximum number of customer UDP ports to send on
    static constexpr uint8_t GDL90OVERUDP_MAX_CUSTOM_CLIENTS = 4;                                 // Maximum custom clients
    static constexpr uint16_t NUM_GDL_PACKETS = static_cast<int>(512 / sizeof(OpenAce::GDLData)); // Number of GDL packets that can be buffered
    static constexpr uint16_t UDP_BUFFER_SIZE = NUM_GDL_PACKETS * sizeof(OpenAce::GDLData);

    enum TaskState : uint32_t
    {
        EXIT = 1 << 0,
        TRANSMIT = 1 << 2,
    };

    mutable struct
    {
        uint32_t heartbeatTx = 0;
        uint32_t bufferAllocErr = 0;
        uint32_t sendFailureErr = 0;
    } statistics;

    struct ClientConfig
    {
        uint32_t ip = IPADDR_NONE;
        uint16_t port = 0;
    };

    udp_pcb *pcb;
    uint32_t networkAddress;
    TaskHandle_t taskHandle;
    SemaphoreHandle_t mutex;
    etl::list<ClientConfig, GDL90OVERUDP_MAX_CUSTOM_CLIENTS> customClients;
    etl::set<uint32_t, OPENACE_MAXIMUM_TCP_CLIENTS> connectedClients;
    etl::set<uint16_t, GDL90OVERUDP_MAX_PORTS> udpPorts = {};

    CircularBuffer<UDP_BUFFER_SIZE> gdlDataBuffer;

private:
    void getConfiguration(const Configuration &config);
    void getConfigurationNoMutex(const Configuration &config);

public:
    static constexpr const etl::string_view NAME = "GDLoverUDP";
    GDLoverUDP(etl::imessage_bus &bus, const Configuration &config) : BaseModule(bus, NAME),
                                                                      pcb(nullptr),
                                                                      networkAddress(0)
    {
        printf("Size of OpenAce::GDLData: %zu\n", sizeof(OpenAce::GDLData));
        getConfigurationNoMutex(config);
    }

    virtual ~GDLoverUDP() = default;

    virtual OpenAce::PostConstruct postConstruct() override;

    virtual void getData(etl::string_stream &stream, const etl::string_view path) const override;

    virtual void start() override;

    virtual void stop() override;

    void on_receive_unknown(const etl::imessage &msg);

    void on_receive(const OpenAce::AccessPointClientsMsg &msg);

    void on_receive(const OpenAce::WifiConnectionStateMsg &msg);

    void on_receive(const OpenAce::GdlMsg &msg);

    void on_receive(const OpenAce::ConfigUpdatedMsg &msg);

    static void gdlOverUDPTask(void *arg);

    void transmitBuffer();

    void sendTo(const char *part, size_t size, uint32_t ip, int16_t port);
};
