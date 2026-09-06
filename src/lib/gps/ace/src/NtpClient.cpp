#include "../NtpClient.hpp"

#include <cstring>

#include "ace/coreutils.hpp"
#include "ace/lwipraiilock.hpp"
#include "ace/scopedpbuf.hpp"

#include "lwip/def.h"
#include "lwip/dns.h"
#include "lwip/pbuf.h"
#include "lwip/udp.h"

NtpClient::NtpClient(TimeCallback timeCallback_, PpsCallback ppsCallback_, FailureCallback failureCallback_)
    : timeCallback(timeCallback_),
      ppsCallback(ppsCallback_),
      failureCallback(failureCallback_)
{
    GATAS_ASSERT(timeCallback.is_valid(), "NtpClient requires a time callback");
    GATAS_ASSERT(ppsCallback.is_valid(), "NtpClient requires a PPS callback");
    GATAS_ASSERT(failureCallback.is_valid(), "NtpClient requires a failure callback");

    LwipRAIILock lock;
    pcb = udp_new_ip_type(IPADDR_TYPE_ANY);
    if (pcb)
    {
        udp_recv(pcb, receiveCallback, this);
    }
}

NtpClient::~NtpClient()
{
    GATAS_ASSERT(!processPending, "NtpClient cannot be destroyed during a DNS lookup");
    if (pcb)
    {
        LwipRAIILock lock;
        udp_remove(pcb);
    }
}

void NtpClient::setServerName(etl::string_view serverName_)
{
    server = serverName_;
}

NtpClient::ServerName NtpClient::serverName() const
{
    return server;
}

bool NtpClient::requestTime()
{
    // Fail Safe: Reset timer after 10 minutes, if something odd happaned and processPending was never reset
    if ((CoreUtils::monotonic32() - ntpRequestSendUs) > 10 * 60 * 1'000'000)
    {
        processPending = false;
    }

    if (pcb == nullptr || processPending || server.empty())
    {
        GATAS_INFO("processPending, pcb or server.empty() ");
        return false;
    }

    LwipRAIILock lock;
    ip_addr_t address;
    processPending = true;
    const err_t error = dns_gethostbyname(server.c_str(), &address, dnsCallback, this);
    if (error == ERR_OK)
    {
        sendRequest(&address);
    }
    else if (error == ERR_INPROGRESS)
    {
        // waiting for DNS resturn, then we call sendRequest
    }
    else
    {
        failRequest();
        return false;
    }

    return true;
}

void NtpClient::dnsCallback(const char *name, const ip_addr_t *address, void *arg)
{
    (void)name;
    auto *client = static_cast<NtpClient *>(arg);

    if (address == nullptr)
    {
        // When address is null, the DNS resolcing failed
        client->failRequest();
        return;
    }

    client->sendRequest(address);
}

void NtpClient::sendRequest(const ip_addr_t *address)
{
    GATAS_INFO("Resolved NTP server %s -> %u.%u.%u.%u", server.c_str(), ip4_addr1(address), ip4_addr2(address), ip4_addr3(address), ip4_addr4(address));

    if (address == nullptr)
    {
        failRequest();
        return;
    }

    pbuf *packet = pbuf_alloc(PBUF_TRANSPORT, NTP_PACKET_SIZE, PBUF_POOL);
    ScopedPbuf scopedPbuf(packet);
    if (packet == nullptr)
    {
        failRequest();
        return;
    }

    uint8_t request[NTP_PACKET_SIZE] = {};
    request[0] = 0x23; // NTP v4, client mode
    if (pbuf_take(packet, request, sizeof(request)) != ERR_OK)
    {
        failRequest();
        return;
    }

    ntpRequestSendUs = CoreUtils::monotonic32();

    const err_t error = udp_sendto(pcb, packet, address, NTP_PORT);
    if (error != ERR_OK)
    {
        failRequest();
    }
}

void NtpClient::receiveCallback(void *arg, udp_pcb *pcb_, pbuf *packet, const ip_addr_t *address, uint16_t port)
{
    (void)address;
    ScopedPbuf scopedPbuf(packet);
    const uint32_t receiveUs = CoreUtils::monotonic32();

    auto *client = static_cast<NtpClient *>(arg);

    if (packet == nullptr)
    {
        if (client != nullptr)
        {
            client->failRequest(Failure::REQUEST);
        }
        return;
    }

    // sanety checks
    if (!client->processPending || pcb_ != client->pcb || port != NTP_PORT)
    {
        return;
    }

    uint8_t response[NTP_PACKET_SIZE] = {};
    const bool responseCopied = packet->tot_len >= NTP_PACKET_SIZE && pbuf_copy_partial(packet, response, sizeof(response), 0) == sizeof(response);

    uint32_t ntpSecondsNetwork = 0;
    uint32_t ntpFractionNetwork = 0;
    std::memcpy(&ntpSecondsNetwork, response + 40, sizeof(ntpSecondsNetwork));
    std::memcpy(&ntpFractionNetwork, response + 44, sizeof(ntpFractionNetwork));
    const uint32_t ntpSeconds = lwip_ntohl(ntpSecondsNetwork);
    const uint32_t ntpFraction = lwip_ntohl(ntpFractionNetwork);
    const uint8_t version = static_cast<uint8_t>((response[0] >> 3) & 0x07);

    const bool valid = responseCopied &&
                       version >= 3 && version <= 4 &&
                       (response[0] & 0x07) == 4 && // server mode
                       (response[0] >> 6) != 3 &&   // clock is synchronized
                       response[1] >= 1 && response[1] <= 15 &&
                       ntpSeconds != 0;

    if (!valid)
    {
        // Valid might be fails if a request was not even send from us
        client->processPending = false;
        return;
    }

    const uint64_t unixSeconds = ntpSeconds >= NTP_TO_UNIX_EPOCH_SECONDS
                                     ? static_cast<uint64_t>(ntpSeconds - NTP_TO_UNIX_EPOCH_SECONDS)
                                     : (1ULL << 32) + ntpSeconds - NTP_TO_UNIX_EPOCH_SECONDS;

    const uint32_t roundTripMs = (receiveUs - client->ntpRequestSendUs) / 1'000ULL;
    const uint64_t unixMs = unixSeconds * 1'000ULL +
                            ((static_cast<uint64_t>(ntpFraction) * 1'000ULL) >> 32) +
                            roundTripMs / 2;

    // Maximum jitter on network allowed
    if (roundTripMs > MAX_ROUND_TRIP_MS)
    {
        client->failRequest(Failure::ROUND_TRIP_TOO_LONG);
        return;
    }

    client->ppsCallback(static_cast<int32_t>((unixMs % 1'000ULL) * 1'000ULL));
    client->timeCallback(NtpTimeResult{unixMs, receiveUs});
    client->processPending = false;
}

void NtpClient::failRequest(Failure failure)
{
    failureCallback(failure);
    processPending = false;
}
