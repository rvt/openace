#include "../NtpClient.hpp"

#include <cstring>

#include "ace/coreutils.hpp"
#include "ace/lwiplock.hpp"

#include "lwip/def.h"
#include "lwip/dns.h"
#include "lwip/pbuf.h"
#include "lwip/udp.h"

NtpClient::NtpClient(TimeCallback timeCallback_, PpsCallback ppsCallback_, FailureCallback failureCallback_)
    : timeCallback(timeCallback_),
      ppsCallback(ppsCallback_),
      failureCallback(failureCallback_)
{
}

NtpClient::~NtpClient()
{
    cancel();
}

void NtpClient::setServerName(etl::string_view serverName_)
{
    server.assign(serverName_.begin(), serverName_.end());
}

etl::string_view NtpClient::serverName() const
{
    return server;
}

bool NtpClient::requestTime()
{
    if (busy() || server.empty())
    {
        return false;
    }

    LwipLock lock;
    pcb = udp_new_ip_type(IPADDR_TYPE_ANY);
    if (pcb == nullptr)
    {
        if (failureCallback.is_valid())
        {
            failureCallback();
        }
        return false;
    }

    udp_recv(pcb, receiveCallback, this);
    active = true;
    requestStartedUs = CoreUtils::monotonic();

    ip_addr_t address;
    const err_t error = dns_gethostbyname(server.c_str(), &address, dnsCallback, this);
    if (error == ERR_OK)
    {
        sendRequest(&address);
    }
    else if (error == ERR_INPROGRESS)
    {
        dnsPending = true;
    }
    else
    {
        failRequest();
    }

    return active;
}

void NtpClient::cancel()
{
    LwipLock lock;
    closePcb();
    active = false;
}

void NtpClient::poll(uint64_t nowUs)
{
    if (active && nowUs >= requestStartedUs && nowUs - requestStartedUs >= REQUEST_TIMEOUT_US)
    {
        cancel();
        if (failureCallback.is_valid())
        {
            failureCallback();
        }
    }
}

bool NtpClient::busy() const
{
    return active || dnsPending;
}

void NtpClient::dnsCallback(const char *name, const ip_addr_t *address, void *arg)
{
    (void)name;
    auto *client = static_cast<NtpClient *>(arg);
    if (client == nullptr)
    {
        return;
    }

    client->dnsPending = false;
    if (!client->active)
    {
        return;
    }

    if (address == nullptr)
    {
        client->failRequest();
        return;
    }

    client->sendRequest(address);
}

void NtpClient::sendRequest(const ip_addr_t *address)
{
    if (!active || pcb == nullptr || address == nullptr)
    {
        failRequest();
        return;
    }

    pbuf *packet = pbuf_alloc(PBUF_TRANSPORT, NTP_PACKET_SIZE, PBUF_RAM);
    if (packet == nullptr)
    {
        failRequest();
        return;
    }

    uint8_t request[NTP_PACKET_SIZE] = {};
    request[0] = 0x1B; // NTP v3, client mode
    if (pbuf_take(packet, request, sizeof(request)) != ERR_OK ||
        udp_connect(pcb, address, NTP_PORT) != ERR_OK)
    {
        pbuf_free(packet);
        failRequest();
        return;
    }

    requestSentUs = CoreUtils::monotonic();
    const err_t error = udp_send(pcb, packet);
    pbuf_free(packet);
    if (error != ERR_OK)
    {
        failRequest();
    }
}

void NtpClient::receiveCallback(void *arg, udp_pcb *pcb_, pbuf *packet, const ip_addr_t *address, uint16_t port)
{
    (void)address;
    auto *client = static_cast<NtpClient *>(arg);
    if (client == nullptr || packet == nullptr)
    {
        if (packet != nullptr)
        {
            pbuf_free(packet);
        }
        return;
    }

    uint8_t response[NTP_PACKET_SIZE] = {};
    const bool valid = client->active && client->requestSentUs != 0 && port == NTP_PORT &&
                       packet->tot_len >= NTP_PACKET_SIZE &&
                       pbuf_copy_partial(packet, response, sizeof(response), 0) == sizeof(response) &&
                       (response[0] & 0x07) == 4 && // server mode
                       (response[0] >> 6) != 3 &&   // clock is synchronized
                       response[1] != 0;            // valid stratum
    pbuf_free(packet);

    if (!valid)
    {
        return;
    }

    uint32_t ntpSecondsNetwork = 0;
    uint32_t ntpFractionNetwork = 0;
    std::memcpy(&ntpSecondsNetwork, response + 40, sizeof(ntpSecondsNetwork));
    std::memcpy(&ntpFractionNetwork, response + 44, sizeof(ntpFractionNetwork));
    const uint32_t ntpSeconds = lwip_ntohl(ntpSecondsNetwork);
    const uint32_t ntpFraction = lwip_ntohl(ntpFractionNetwork);
    const uint64_t unixSeconds = ntpSeconds >= NTP_TO_UNIX_EPOCH_SECONDS
                                     ? static_cast<uint64_t>(ntpSeconds - NTP_TO_UNIX_EPOCH_SECONDS)
                                     : (1ULL << 32) + ntpSeconds - NTP_TO_UNIX_EPOCH_SECONDS;

    const uint64_t receiveUs = CoreUtils::monotonic();
    const uint64_t roundTripMs = (receiveUs - client->requestSentUs) / 1'000ULL;
    const uint64_t unixMs = unixSeconds * 1'000ULL +
                            ((static_cast<uint64_t>(ntpFraction) * 1'000ULL) >> 32) +
                            roundTripMs / 2;

    client->active = false;
    client->requestSentUs = 0;
    if (pcb_ != nullptr)
    {
        udp_remove(pcb_);
        client->pcb = nullptr;
    }

    if (client->ppsCallback.is_valid())
    {
        client->ppsCallback(static_cast<int32_t>((unixMs % 1'000ULL) * 1'000ULL));
    }
    if (client->timeCallback.is_valid())
    {
        client->timeCallback(unixMs);
    }
}

void NtpClient::closePcb()
{
    if (pcb != nullptr)
    {
        udp_remove(pcb);
        pcb = nullptr;
    }
    requestSentUs = 0;
}

void NtpClient::failRequest()
{
    closePcb();
    active = false;
    if (failureCallback.is_valid())
    {
        failureCallback();
    }
}
