#pragma once

#include <stdint.h>

#include "pico/cyw43_arch.h"
#include "lwip/pbuf.h"
#include "lwip/tcp.h"

#include "etl/map.h"
#include "etl/message_bus.h"
#include "etl/function.h"

#include "ace/constants.hpp"
#include "ace/messagerouter.hpp"
#include "ace/basemodule.hpp"
#include "ace/messages.hpp"

/**
 * Client that can connect to a host and a port and expect to receive line terminated NMEA Messages
 */
template <size_t lineLength>
class TcpClient
{
    constexpr static uint32_t BUF_SIZE = lineLength * 2 + 2 * lineLength + 1;
    constexpr static uint32_t RECEIVE_BUFFER_SIZE = 1024;

public:
    using CallBackFunction = etl::delegate<void(const char *)>;

private:
    bool tcp_client_open()
    {
        // puts("TcpClient: tcp_client_open");
        ip_addr_t remote_addr = IPADDR4_INIT(ipPort.ip);
        tcp_pcb = tcp_new_ip_type(IP_GET_TYPE(&remote_addr));
        if (!tcp_pcb)
        {
            printf("TcpClient: failed to create PCB\n");
            return false;
        }

        tcp_arg(tcp_pcb, this);
        tcp_recv(tcp_pcb, tcp_client_recv);
        tcp_err(tcp_pcb, tcp_client_err);
        partialLength = 0;

        // cyw43_arch_lwip_begin/end should be used around calls into lwIP to ensure correct locking.
        // You can omit them if you are in a callback from lwIP. Note that when using pico_cyw_arch_poll
        // these calls are a no-op and can be omitted, but it is a good practice to use them in
        // case you switch the cyw43_arch type later.
        cyw43_arch_lwip_begin();
        err_t err = tcp_connect(tcp_pcb, &remote_addr, ipPort.port, tcp_client_connected);
        cyw43_arch_lwip_end();

        return err == ERR_OK;
    }

    static void tcp_client_close(void *arg)
    {
        TcpClient *tcpClient = (TcpClient *)arg;
        // puts("TcpClient: tcp_client_close");
        if (tcpClient->tcp_pcb != nullptr)
        {

            tcp_err(tcpClient->tcp_pcb, nullptr);
            tcp_recv(tcpClient->tcp_pcb, nullptr);
            tcp_arg(tcpClient->tcp_pcb, nullptr);

            err_t err = tcp_close(tcpClient->tcp_pcb);
            if (err != ERR_OK)
            {
                printf("TcpClient: close failed %d, calling abort\n", err);
                tcp_abort(tcpClient->tcp_pcb);
            }

            tcpClient->tcp_pcb = nullptr;
        }
    }

    static void tcp_client_err(void *arg, err_t err)
    {
        // printf("TcpClient: tcp_client_err %d\n", err);
        // In all cases where tcp_client_err is called back, the PCB will be cleaned up
        TcpClient *tcpClient = (TcpClient *)arg;
        if (err != ERR_MEM)
        {
            tcpClient->tcp_pcb = nullptr;
        }
    }

    typedef err_t (*tcp_connected_fn)(void *arg, struct tcp_pcb *tpcb, err_t err);
    static err_t tcp_client_connected(void *arg, struct tcp_pcb *tpcb, err_t err)
    {
        (void)tpcb;
        if (err != ERR_OK)
        {
            tcp_client_close(arg);
            return err;
        }
        // puts("TcpClient: Connected\n");

        return ERR_OK;
    }

    static err_t tcp_client_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *pBuf, err_t err)
    {
        if (arg == nullptr)
        {
            tcp_client_close(arg);
            return ERR_ARG;
        }
        TcpClient *tcpClient = (TcpClient *)arg;

        if (err != ERR_OK)
        {
            tcp_client_close(arg);
            return ERR_OK;
        }

        if (pBuf == nullptr)
        {
            tcp_client_close(arg);
            return ERR_OK;
        }

        // this method is callback from lwIP, so cyw43_arch_lwip_begin is not required, however you
        // can use this method to cause an assertion in debug mode, if this method is called when
        // cyw43_arch_lwip_begin IS needed
        cyw43_arch_lwip_check();
        if (pBuf->tot_len > 0)
        {
            uint8_t internalBuffer[RECEIVE_BUFFER_SIZE];
            memcpy(internalBuffer, tcpClient->partialBuffer, tcpClient->partialLength);

            uint16_t pBufBytesOffset = 0;
            int32_t pBufBytesLeft = pBuf->tot_len;
            for (;;)
            {
                const uint16_t bytesBufferLeft = RECEIVE_BUFFER_SIZE - tcpClient->partialLength;
                uint8_t *bufferPtr = internalBuffer + tcpClient->partialLength;

                const uint16_t maxCopySize = (pBufBytesLeft > bytesBufferLeft) ? bytesBufferLeft : pBufBytesLeft;
                uint16_t bytesCopied = pbuf_copy_partial(pBuf, bufferPtr, maxCopySize, pBufBytesOffset);

                pBufBytesOffset += bytesCopied;
                pBufBytesLeft -= bytesCopied;

                // Process the received possible partial buffer
                uint16_t bytesInternalBuffer = bytesCopied + tcpClient->partialLength;
                uint16_t bytesProcessed = tcpClient->processBuffer(internalBuffer, bytesInternalBuffer);

                // Copy back into partial buffer for next round
                tcpClient->partialLength = bytesInternalBuffer - bytesProcessed;

                if (pBufBytesLeft > 0)
                {
                    // printf("memmove left:%d", tcpClient->partialLength);
                    memmove(internalBuffer, internalBuffer + bytesProcessed, tcpClient->partialLength);
                }
                else
                {
                    // printf("memcpy left:%d", tcpClient->partialLength);
                    memcpy(tcpClient->partialBuffer, internalBuffer + bytesProcessed, tcpClient->partialLength);
                    break;
                }
            };
        }
        tcp_recved(tpcb, pBuf->tot_len);
        pbuf_free(pBuf);

        return ERR_OK;
    }

    uint16_t processBuffer(uint8_t internalBuffer[], uint16_t length) const
    {
        uint8_t *current = internalBuffer;
        uint8_t *buffer_end = internalBuffer + length;
        do
        {
            // Find the start of a packet within the received length
            uint8_t *start = static_cast<uint8_t *>(memchr(current, '*', buffer_end - current));
            if (!start)
                break;

            // Find the end of a packet within the received length
            uint8_t *end = static_cast<uint8_t *>(memchr(start, ';', buffer_end - start));
            if (!end)
                break;

            end[1] = '\0';
            // puts(reinterpret_cast<const char *>(start));
            callback(reinterpret_cast<const char *>(start));

            // Move the current pointer past the end of the packet
            current = end + 1;
        }
        while (current < buffer_end);
        return current - internalBuffer;
    }

private:
    OpenAce::Config::IpPort ipPort;
    struct tcp_pcb *tcp_pcb;
    uint8_t partialBuffer[BUF_SIZE]; // Minimum room for two sentences and additional \r\n characters
    uint16_t partialLength;
    CallBackFunction callback;

public:
    // @techdebt: Have a handler with a lambda?
    TcpClient(OpenAce::Config::IpPort ipPort_, CallBackFunction callback_) : ipPort(ipPort_),
        tcp_pcb(nullptr),
        partialLength(0),
        callback(callback_)
    {
    }

    virtual ~TcpClient()
    {
    };

    OpenAce::PostConstruct postConstruct()
    {
        if (ipPort.ip == IPADDR_NONE)
        {
            return OpenAce::PostConstruct::CONFIG_ERROR;
        }

        return OpenAce::PostConstruct::OK;
    }

    void start()
    {
        tcp_client_open();
    };

    void stop()
    {
        tcp_client_close(this);
    };

    bool isStopped() const
    {
        return tcp_pcb == nullptr;
    }
};
