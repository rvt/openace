#pragma once

#include "etl/array.h"
#include "etl/span.h"
#include "etl/delegate.h"
#include "etl/array_view.h"
#include "tcpclient.hpp"
#include "delimiterbitmap.hpp"
#include "packetbuffer.hpp"

template <typename Derived>
class StreamLineParser
{
public:
    using CallBackFunction = etl::delegate<void(etl::span<uint8_t>)>;

protected:
    etl::array_view<uint8_t> receiveBuffer;
    size_t receiveBufferPointer = 0;
    CallBackFunction lineCallback;
    const DelimiterBitmap &delimiters;

public:
    StreamLineParser(etl::array_view<uint8_t> ib, CallBackFunction cb, const DelimiterBitmap &_delimiters)
        : receiveBuffer(ib), lineCallback(cb), delimiters(_delimiters)
    {
    }

    // Called by derived class when new data arrives
    void onRawData(etl::span<uint8_t> data)
    {
        for (uint8_t c : data)
        {
            if (delimiters.contains(c))
            {
                if (receiveBufferPointer > 0)
                {
                    receiveBuffer[receiveBufferPointer++] = 0;
                    lineCallback({receiveBuffer.data(), receiveBufferPointer});
                    receiveBufferPointer = 0;
                }
            }
            else
            {
                if (receiveBufferPointer >= receiveBuffer.size())
                {
                    receiveBufferPointer = 0;
                }
                receiveBuffer[receiveBufferPointer++] = c;
            }
        }
    }
};

template <size_t RECEIVE_BUFFER_SIZE = 32, size_t SEND_BUFFER_SIZE = 32, size_t SEND_PACKETS = 2>
class TcpBufferedClient : public StreamLineParser<TcpBufferedClient<RECEIVE_BUFFER_SIZE, SEND_BUFFER_SIZE, SEND_PACKETS>>
{
private:
    TcpClient client;
    etl::array<uint8_t, RECEIVE_BUFFER_SIZE> receiveBuffer;
    PacketBuffer<SEND_BUFFER_SIZE, SEND_PACKETS> sendBuffer;
    ;
    using Base = StreamLineParser<TcpBufferedClient<RECEIVE_BUFFER_SIZE, SEND_BUFFER_SIZE>>;

public:
    using typename Base::CallBackFunction;

    TcpBufferedClient(GATAS::Config::IpPort ipPort,
                      const DelimiterBitmap &delimiters,
                      CallBackFunction cb)
        : Base(receiveBuffer, cb, delimiters),
          client(ipPort,
                 TcpClient::ReceiveHandler::create<TcpBufferedClient, &TcpBufferedClient::handleIncommingData>(*this),
                 TcpClient::SentHandler::create<TcpBufferedClient, &TcpBufferedClient::handleSendData>(*this))
    {
    }

    void start()
    {
        client.start();
    }
    void stop()
    {
        client.stop();
    }
    bool write(etl::span<uint8_t> data)
    {
        if (sendBuffer.packets())
        {
            sendBuffer.set(data);
        }
        else
        {
            // TODO: See if we want to send larger buffers instead of what is in data
            sendBuffer.set(data);
            client.write(data);
            // Read this single package to ensure flow control, this can happen when this function is called before sent is called
            // and this we first need to buffer it bedore the next send can happen
            sendBuffer.read(1);
        }
        return true;
    }
    GATAS::PostConstruct postConstruct() {
        return GATAS::PostConstruct::OK;
    }

    bool isConnected() const
    {
        return client.isConnected();
    }
    
    bool isDisconnected() const
    {
        return client.isDisconnected();
    }
    void reconnect() 
    {
        client.reconnect();
    }
    uint32_t ip() const
    {
        return client.ip();
    }

private:
    void handleIncommingData(etl::span<uint8_t> data)
    {
        this->onRawData(data);
    }

    void handleSendData(uint16_t len)
    {
        (void)len;
        sendBuffer.compact();
        auto oData = sendBuffer.read(512);
        if (oData)
        {
            auto data = oData.value();
            client.write(data);
        }
    }
};
