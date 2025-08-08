#include <stdio.h>

#include "../gatasconnect.hpp"
#include "ace/coreutils.hpp"
#include "ace/binarymessages.hpp"
#include "ace/cobs.hpp"

/* LwIP */
#include "lwip/ip_addr.h"
#include "pico/cyw43_arch.h"
#include "lwip/pbuf.h"

GATAS::PostConstruct GatasConnect::postConstruct()
{
    requestTimer = xTimerCreate(GatasConnect::NAME,
                                TASK_DELAY_MS(250),
                                pdFALSE,
                                this,
                                requestTimerCallback);

    if (requestTimer == nullptr)
    {
        return GATAS::PostConstruct::TIMER_ERROR;
    }
    pcbSend = udp_new_ip_type(IPADDR_TYPE_ANY);
    if (pcbSend == nullptr)
    {
        xTimerDelete(requestTimer, portMAX_DELAY);
        return GATAS::PostConstruct::NETWORK_ERROR;
    }
    udp_recv(pcbSend, receiveUdpMessage, this);

    return GATAS::PostConstruct::OK;
}

void GatasConnect::start()
{
    xTimerChangePeriod(requestTimer, TASK_DELAY_MS(1000), portMAX_DELAY);
    getBus().subscribe(*this);
};

void GatasConnect::stop()
{
    getBus().unsubscribe(*this);
};

void GatasConnect::getData(etl::string_stream &stream, const etl::string_view path) const
{
    (void)path;
    stream << "{";
    stream << "\"msgSend\":" << statistics.msgSend;
    stream << ",\"msgReceived\":" << statistics.msgReceived;
    stream << ",\"bufferAllocErr\":" << statistics.bufferAllocErr;
    stream << "}\n";
}

void GatasConnect::on_receive_unknown(const etl::imessage &msg)
{
    (void)msg;
}

/**
 * Track wifi connects and disconnect
 */
void GatasConnect::on_receive(const GATAS::WifiConnectionStateMsg &wcs)
{
    wifiConnected = wcs.connected;
}

/**
 * Starts and stop LWiP when wifi connects or disconnects to cleanup any resources
 */
void GatasConnect::on_receive(const GATAS::IdleMsg &msg)
{
    (void)msg;
    if (wifiConnected)
    {
        xTimerChangePeriod(requestTimer, TASK_DELAY_MS(1000), portMAX_DELAY);
    }
    else
    {
        xTimerStop(requestTimer, portMAX_DELAY);
    }
}

void GatasConnect::on_receive(const GATAS::OwnshipPositionMsg &msg)
{
    ownshipPosition.store(msg.position, etl::memory_order_release);
}

void GatasConnect::on_receive(const GATAS::ConfigUpdatedMsg &msg)
{

    if (msg.moduleName == GatasConnect::NAME)
    {
        gatasServer = msg.config.ipPortBypath(NAME, "gatasServer");
    }
}

void GatasConnect::receiveUdpMessage(void *arg, struct udp_pcb *pcb,
                                     struct pbuf *p, const ip_addr_t *addr, u16_t port)
{
    (void)arg;
    (void)pcb;
    //    (void)p;
    (void)addr;
    (void)port;
    GatasConnect *taskCtx = (GatasConnect *)(arg);
    taskCtx->statistics.msgReceived += 1;

    auto payload = etl::span<uint8_t>(reinterpret_cast<uint8_t *>(p->payload), p->tot_len);
    auto offset = payload.begin();
    while (true)
    {
        auto result = etl::find(offset, payload.end(), 0);

        // End reached
        if (result == payload.end())
        {
            break;
        }

        // When we hit a 0 byte
        auto length = result - offset;
        if (length <= 1)
        {
            offset = offset + length + 1;
            continue;
        }

        // // Decode in-place
        size_t decodedSize = decodeCOBS_inplace(offset, length);

        etl::bit_stream_reader reader(offset, decodedSize, etl::endian::big);
        uint8_t frameType = reader.read_unchecked<uint8_t>(8U);
        if (frameType == BinaryMessages::DataType::AIRCRAFT_POSITION_TYPE_V1)
        {
            reader.restart();
            taskCtx->getBus().receive(GATAS::AircraftPositionMsg(BinaryMessages::AircraftPositionInfo(reader)));
        }
        offset += decodedSize + 1;
    }

    pbuf_free(p);
}

/**
 * Prepare a position request to the server, the serveer will then send aircraft information back
 */
void GatasConnect::requestTimerCallback(TimerHandle_t xTimer)
{
    GatasConnect *taskCtx = (GatasConnect *)pvTimerGetTimerID(xTimer);

    // Write the ownship
    std::array<uint8_t, 26 + 2> storage; // MSG + CRC
    etl::bit_stream_writer writer(storage.data(), storage.size(), etl::endian::big);
    BinaryMessages::fromOwnshipPositionInfo(writer, taskCtx->ownshipPosition.load(etl::memory_order_acquire));
    auto checksum = BinaryMessages::binaryMsgChecksum(etl::span<uint8_t>(storage.data(), writer.size_bytes()));
    writer.write(checksum, 16U);

    // Transform to Cobs
    std::array<uint8_t, storage.size() + 1 + 1> cobsstorage; // 1Byte for COBS Zero padding, 1 Byte because datasize is < 256byte for zero termination
    auto size = encodeCOBS(storage.data(), writer.size_bytes(), cobsstorage.data(), cobsstorage.size(), true);

    // Send to the server
    auto server = taskCtx->gatasServer;
    ip_addr_t addr;
    ip4_addr_set_u32(&addr, server.ip);
    cyw43_arch_lwip_begin();
    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, size, PBUF_RAM);
    if (p == nullptr)
    {
        taskCtx->statistics.bufferAllocErr += 1;
        cyw43_arch_lwip_end();
        return;
    }
    memcpy((char *)p->payload, cobsstorage.data(), size);
    auto err = udp_sendto(taskCtx->pcbSend, p, &addr, server.port);
    pbuf_free(p);
    cyw43_arch_lwip_end();
    xTimerChangePeriod(taskCtx->requestTimer, TASK_DELAY_MS(1000), portMAX_DELAY);

    if (err != ERR_OK)
    {
        taskCtx->statistics.msgSendFailed += 1;
        return;
    }
    taskCtx->statistics.msgSend += 1;
    // Request a new position in one second
}
