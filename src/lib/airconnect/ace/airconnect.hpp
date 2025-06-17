#pragma once

#include <stdint.h>

/* FreeRTOS */
#include "FreeRTOS.h"
#include "task.h"

/* LwIP */
#include "lwip/pbuf.h"
#include "lwip/tcp.h"
#include "pico/cyw43_arch.h"

/* ETLCPP */
#include "etl/message_bus.h"
#include "etl/list.h"
#include "etl/string.h"

/* GaTas */
#include "ace/constants.hpp"
#include "ace/messagerouter.hpp"
#include "ace/basemodule.hpp"
#include "ace/messages.hpp"
#include "ace/circularbuffer.hpp"

/**
 * AirConnect protocol for EFB's that only support AirConnect.
 * THis protocol is currently not recommende if you can also use GDL90, if you need to use NMEA and have BLE, use that
 */
class AirConnect : public BaseModule, public etl::message_router<AirConnect, GATAS::DataPortMsg, GATAS::WifiConnectionStateMsg, GATAS::IdleMsg>
{
    friend class message_router;
    static constexpr uint16_t AIRCONNECT_PORT = 2000;
    static constexpr uint16_t BUFFER_SIZE = 1024; // TODO: Tune buffer
    struct
    {
        uint32_t toManyClients = 0;
        uint16_t tcpWriteErr = 0;
        uint32_t newClientConnection = 0;
    } statistics;

    struct TcpClientState
    {
        tcp_pcb *pcb;
        AirConnect *airConnect;
        uint16_t bufferOverrunErr;
        CircularBuffer<BUFFER_SIZE> buffer;

        // Constructor
        TcpClientState() = default;
        TcpClientState(tcp_pcb *pcb_, AirConnect *airConnect_) : pcb(pcb_),
                                                               airConnect(airConnect_),
                                                               bufferOverrunErr(0)
        {
        }

            // Delete copy constructor and copy assignment operator
        TcpClientState(const TcpClientState &) = delete;
        TcpClientState &operator=(const TcpClientState &) = delete;

        // (Optional) Delete move constructor and move assignment operator
        TcpClientState(TcpClientState &&) = delete;
        TcpClientState &operator=(TcpClientState &&) = delete;
    };

    using ConnectedClients = etl::list<TcpClientState, GATAS_MAXIMUM_TCP_CLIENTS>;
    ConnectedClients connectedClients;
    tcp_pcb *serverPcb;
    bool wifiConnected;

private:
    virtual GATAS::PostConstruct postConstruct() override;

    virtual void start() override;

    virtual void stop() override;

    void on_receive(const GATAS::DataPortMsg &msg);
    
    void on_receive(const GATAS::WifiConnectionStateMsg &wcs);

    void on_receive(const GATAS::IdleMsg &msg);

    void on_receive_unknown(const etl::imessage &msg);

    //    static err_t tcp_write_data(TcpClientState &con_state, const char *data, uint8_t length);
    static err_t tcp_server_sent(void *arg, struct tcp_pcb *pcb, u16_t len);
    static err_t tcp_server_poll(void *arg, struct tcp_pcb *pcb);
    static err_t tcp_close_client_connection(TcpClientState &con_state, err_t err);
    static err_t tcp_server_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err);
    static void tcp_server_err(void *arg, err_t err);
    static err_t tcp_server_accept(void *arg, tcp_pcb *client_pcb, err_t aerr);

    bool tcp_server_start();
    void tcp_server_close();
    void removeEmptyPCB();
    virtual void getData(etl::string_stream &stream, const etl::string_view path) const override;

    // static void airConnectTask(void *arg);

public:
    static constexpr const char *NAME = "AirConnect";
    AirConnect(etl::imessage_bus &bus, const Configuration &config) : BaseModule(bus, NAME), serverPcb(nullptr), wifiConnected(false)
    {
        (void)config;
    }

    virtual ~AirConnect() = default;
};
