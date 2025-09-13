#pragma once

#include <stdint.h>

#include "ace/lwiplock.hpp"
#include "pico/cyw43_arch.h"
#include "lwip/pbuf.h"
#include "lwip/tcp.h"

#include "etl/function.h"


/**
 * Client that can connect to a host and a port and expect to receive line terminated NMEA Messages
 */
template <size_t RECEIVE_BUFFER_SIZE>
class TcpClient
{
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

        // cyw43_arch_lwip_begin/end should be used around calls into lwIP to ensure correct locking.
        // You can omit them if you are in a callback from lwIP. Note that when using pico_cyw_arch_poll
        // these calls are a no-op and can be omitted, but it is a good practice to use them in
        // case you switch the cyw43_arch type later.
        LwipLock lock;
        err_t err = tcp_connect(tcp_pcb, &remote_addr, ipPort.port, tcp_client_connected);
        return err == ERR_OK;
    }

    static void tcp_client_close(void *arg)
    {
        TcpClient *tcpClient = (TcpClient *)arg;
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
        return ERR_OK;
    }

    static err_t tcp_client_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *pBuf, err_t err)
    {
        if (arg == nullptr || err != ERR_OK || pBuf == nullptr)
        {
            tcp_client_close(arg);
            return ERR_ARG;
        }

        TcpClient *tcpClient = (TcpClient *)arg;

        // this method is callback from lwIP, so cyw43_arch_lwip_begin is not required, however you
        // can use this method to cause an assertion in debug mode, if this method is called when
        // cyw43_arch_lwip_begin IS needed
        cyw43_arch_lwip_check();
        if (pBuf->tot_len > 0)
        {
            uint16_t pBufBytesOffset = 0;

            while (pBufBytesOffset < pBuf->tot_len)
            {
                // Calculate maximum bytes we can copy in this iteration
                uint16_t maxCopySize = RECEIVE_BUFFER_SIZE - tcpClient->bufferPosition;

                // Recover from full buffer
                if (maxCopySize == 0)
                {
                    tcpClient->bufferPosition = 0;
                    maxCopySize = RECEIVE_BUFFER_SIZE;
                }

                // Copy from the pbuf into the buffer
                uint16_t bytesCopied = pbuf_copy_partial(pBuf, tcpClient->internalBuffer + tcpClient->bufferPosition, maxCopySize, pBufBytesOffset);

                // If no bytes were copied, break out of the loop (error or no more data)
                if (bytesCopied == 0)
                {
                    break;
                }

                // Update the buffer position and offset
                tcpClient->bufferPosition += bytesCopied;
                pBufBytesOffset += bytesCopied;

                // Process the buffer
                tcpClient->processBuffer();
            }
        }
        tcp_recved(tpcb, pBuf->tot_len);
        pbuf_free(pBuf);

        return ERR_OK;
    }

    void processBuffer()
    {
        auto bufferEnd = internalBuffer + bufferPosition;
        auto current = internalBuffer;
        do
        {
            // Find the next '\n'
            uint8_t *end = static_cast<uint8_t *>(memchr(current, '\n', bufferEnd - current));
            if (!end)
            {
                // No newline found, partial line detected
                size_t bufferPosition = bufferEnd - current;
                memmove(internalBuffer, current, bufferPosition);
                return;
            }

            // Handle possible '\r\n'
            if (end > current && *(end - 1) == '\r')
            {
                *(end - 1) = '\0';
            }
            else
            {
                *end = '\0';
            }

            // Call the callback with the extracted line
            callback(reinterpret_cast<const char *>(current));

            // Move the current pointer past the processed line
            current = end + 1;

        } while (current < bufferEnd);

        // Adjust buffer position to remove processed data
        bufferPosition = bufferEnd - current;
        if (bufferPosition > 0)
        {
            memmove(internalBuffer, current, bufferPosition);
        }
    }

private:
    GATAS::Config::IpPort ipPort;
    struct tcp_pcb *tcp_pcb;
    uint8_t internalBuffer[RECEIVE_BUFFER_SIZE + 1]; // By adding one byte here, we don't need to do a end of buffer check in processBuffer
    uint16_t bufferPosition;
    CallBackFunction callback;

public:
    // @techdebt: Have a handler with a lambda?
    TcpClient(GATAS::Config::IpPort ipPort_, CallBackFunction callback_) : ipPort(ipPort_),
                                                                             tcp_pcb(nullptr),
                                                                             bufferPosition(0),
                                                                             callback(callback_)
    {
    }

    virtual ~TcpClient() = default;

    GATAS::PostConstruct postConstruct()
    {
        if (ipPort.ip == IPADDR_NONE)
        {
            return GATAS::PostConstruct::CONFIG_ERROR;
        }

        return GATAS::PostConstruct::OK;
    }

    void start()
    {
        bufferPosition = 0;
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

    uint32_t ip() const {
        return ipPort.ip;
    }
};
