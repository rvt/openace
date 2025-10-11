#pragma once

#include <stdint.h>
#include "ace/models.hpp"
#include "ace/lwiplock.hpp"
#include "pico/cyw43_arch.h"
#include "lwip/pbuf.h"
#include "lwip/tcp.h"

#include "etl/delegate.h"
#include "etl/span.h"

class TcpClient
{
    friend class TcpListener;

public:
    using ReceiveHandler = etl::delegate<void(etl::span<uint8_t>)>;
    using SentHandler = etl::delegate<void(uint16_t)>;
    using ConnectHandler = etl::delegate<void(bool)>; // optional

    enum class State
    {
        Disconnected,
        Connecting,
        Connected
    };

private:
    struct tcp_pcb *pcb = nullptr;
    State state = State::Disconnected;

    GATAS::Config::IpPort ipPort;

    ReceiveHandler onReceive;
    SentHandler onSent;
    ConnectHandler onConnect; // optional (can be empty)

public:
    TcpClient(GATAS::Config::IpPort ipPort_,
              ReceiveHandler onReceive_,
              SentHandler onSent_,
              ConnectHandler onConnect_ = ConnectHandler())
        : ipPort(ipPort_),
          onReceive(onReceive_),
          onSent(onSent_),
          onConnect(onConnect_)
    {
    }

    TcpClient(struct tcp_pcb *pcb, ReceiveHandler onReceive_)
        : ipPort(GATAS::Config::IpPort{pcb->remote_ip.addr, pcb->remote_port}), onReceive(onReceive_)
    {
        adopt(pcb);
    }

    ~TcpClient() = default;

    bool start()
    {
        if (state != State::Disconnected)
        {
            return false;
        }

        ip_addr_t remote_addr = IPADDR4_INIT(ipPort.ip);
        pcb = tcp_new_ip_type(IP_GET_TYPE(&remote_addr));
        if (!pcb)
        {
            printf("TcpClient: failed to create PCB\n");
            return false;
        }

        state = State::Connecting;

        tcp_arg(pcb, this);
        tcp_recv(pcb, tcp_client_recv);
        tcp_err(pcb, tcp_client_err);
        tcp_sent(pcb, tcp_client_sent);

        LwipLock lock;
        err_t err = tcp_connect(pcb, &remote_addr, ipPort.port, tcp_client_connected);
        if (err != ERR_OK)
        {
            printf("TcpClient: tcp_connect failed (%d)\n", err);
            tcp_client_close(this);
            return false;
        }

        return true;
    }

    void stop()
    {
        tcp_client_close(this);
    }

    bool isConnected() const
    {
        return state == State::Connected && pcb != nullptr;
    }

    bool isDisconnected() const
    {
        return state == State::Disconnected;
    }

    void reconnect()
    {
        start();
    }

    bool write(etl::span<const uint8_t> data)
    {
        if (state != State::Connected || pcb == nullptr) {
            return false;
        }

        LwipLock lock;
        err_t err = tcp_write(pcb, data.data(), data.size(), TCP_WRITE_FLAG_COPY);
        return err == ERR_OK;
    }

    uint32_t ip() const { return ipPort.ip; }

    void adopt(struct tcp_pcb *existing)
    {
        if (pcb)
        {
            stop();
        }
        pcb = existing;

        tcp_arg(pcb, this);
        tcp_recv(pcb, tcp_client_recv);
        tcp_err(pcb, tcp_client_err);
        tcp_sent(pcb, tcp_client_sent);
        tcp_set_flags(pcb, TF_ACK_NOW); // <-- seems to be a must for airConnect?
        state = State::Connected;
    }

    // Setters for the handlers
    void setReceiveHandler(ReceiveHandler handler)
    {
        onReceive = handler;
    }

    void setSentHandler(SentHandler handler)
    {
        onSent = handler;
    }

    void setConnectHandler(ConnectHandler handler)
    {
        onConnect = handler;
    }

private:
    static void tcp_client_close(void *arg)
    {
        TcpClient *self = static_cast<TcpClient *>(arg);
        if (!self || !self->pcb)
        {
            return;
        }

        LwipLock lock;

        tcp_arg(self->pcb, nullptr);
        tcp_recv(self->pcb, nullptr);
        tcp_err(self->pcb, nullptr);
        tcp_sent(self->pcb, nullptr);

        err_t err = tcp_close(self->pcb);
        if (err != ERR_OK)
        {
            printf("TcpClient: close failed (%d), aborting\n", err);
            tcp_abort(self->pcb);
        }

        self->pcb = nullptr;
        self->state = State::Disconnected;
    }

    static void tcp_client_err(void *arg, err_t err)
    {
        TcpClient *self = static_cast<TcpClient *>(arg);

        printf("TcpClient: error callback (%d)\n", err);
        self->pcb = nullptr;
        self->state = State::Disconnected;
        if (self->onConnect.is_valid())
        {
            self->onConnect(false);
        }
    }

    static err_t tcp_client_connected(void *arg, struct tcp_pcb *tpcb, err_t err)
    {
        TcpClient *self = static_cast<TcpClient *>(arg);

        if (err != ERR_OK)
        {
            printf("TcpClient: connect failed (%d)\n", err);
            tcp_client_close(arg);
            if (self->onConnect.is_valid())
            {
                self->onConnect(false);
            }
            return err;
        }

        self->pcb = tpcb;
        self->state = State::Connected;

        if (self->onConnect.is_valid())
        {
            self->onConnect(true);
        }

        return ERR_OK;
    }

    static err_t tcp_client_sent(void *arg, struct tcp_pcb *pcb, u16_t len)
    {
        (void)pcb;
        TcpClient *self = static_cast<TcpClient *>(arg);
        if (self->onSent.is_valid())
        {
            self->onSent(len);
        }
        return ERR_OK;
    }

    static err_t tcp_client_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *pBuf, err_t err)
    {
        TcpClient *self = static_cast<TcpClient *>(arg);
        if (err != ERR_OK)
        {
            tcp_client_close(arg);
            return ERR_ARG;
        }

        // pBuf == nullptr means connection closed by remote
        if (pBuf == nullptr)
        {
            tcp_client_close(arg);
            return ERR_OK;
        }

        cyw43_arch_lwip_check();

        size_t total = 0;
        for (pbuf *q = pBuf; q != nullptr; q = q->next)
        {
            auto *data = reinterpret_cast<uint8_t *>(q->payload);
            size_t len = q->len;
            if (self->onReceive.is_valid())
            {
                self->onReceive({data, len});
            }
            total += len;
        }

        tcp_recved(tpcb, total);
        pbuf_free(pBuf);

        return ERR_OK;
    }
};

class TcpListener
{
public:
    using AcceptHandler = etl::delegate<void(struct tcp_pcb *pcb)>;

private:
    struct tcp_pcb *listenPcb = nullptr;
    uint16_t port;
    AcceptHandler onAccept;

public:
    explicit TcpListener(uint16_t port_, AcceptHandler onAccept_)
        : port(port_), onAccept(onAccept_) {}

    bool start()
    {
        listenPcb = tcp_new_ip_type(IPADDR_TYPE_V4);
        if (!listenPcb)
        {
            printf("TcpListener: failed to create PCB\n");
            return false;
        }

        LwipLock lock;
        err_t err = tcp_bind(listenPcb, IP_ANY_TYPE, port);
        if (err != ERR_OK)
        {
            printf("TcpListener: bind failed (%d)\n", err);
            tcp_close(listenPcb);
            listenPcb = nullptr;
            return false;
        }

        listenPcb = tcp_listen_with_backlog(listenPcb, 4);
        if (!listenPcb)
        {
            printf("TcpListener: listen failed\n");
            return false;
        }

        tcp_arg(listenPcb, this);
        tcp_accept(listenPcb, &TcpListener::on_accept_static);

        printf("TcpListener: listening on port %u\n", port);
        return true;
    }

    void stop()
    {
        if (!listenPcb)
            return;

        LwipLock lock;
        tcp_close(listenPcb);
        listenPcb = nullptr;
    }

private:
    static err_t on_accept_static(void *arg, struct tcp_pcb *new_pcb, err_t err)
    {
        TcpListener *self = static_cast<TcpListener *>(arg);
        if (!self || err != ERR_OK || !new_pcb)
            return ERR_VAL;

        // TcpClient *client = new TcpClient(
        //     {.ip = ip_addr_get_ip4_u32(&new_pcb->remote_ip),
        //      .port = new_pcb->remote_port},
        //     {}, {}, {}); // callbacks to be set by caller later

        // client->adopt(new_pcb); // new method to attach an existing PCB

        if (self->onAccept.is_valid())
        {
            self->onAccept(new_pcb);
            return ERR_OK;
        }

        return ERR_VAL;
    }
};
