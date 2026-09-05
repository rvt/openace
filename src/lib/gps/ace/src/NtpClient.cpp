#include "../NtpClient.hpp"

#include <cstring>

#include "ace/coreutils.hpp"
#include "ace/lwipraiilock.hpp"

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
    GATAS_ASSERT(!dnsPending, "NtpClient cannot be destroyed during a DNS lookup");
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
    // Reset timer after 10 minutes, if something odd happaned
    if ((CoreUtils::monotonic32() - ntpRequestSendUs) > 10 * 60 * 1'000'000)
    {
        ntpRequestSendUs = 0;
        dnsPending = false;
    }

    if (pcb == nullptr || dnsPending || server.empty() || ntpRequestSendUs)
    {
        return false;
    }

    LwipRAIILock lock;
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

    return true;
}

void NtpClient::dnsCallback(const char *name, const ip_addr_t *address, void *arg)
{
    (void)name;
    auto *client = static_cast<NtpClient *>(arg);

    client->dnsPending = false;
    if (address == nullptr)
    {
        client->failRequest();
        return;
    }

    client->sendRequest(address);
}

void NtpClient::sendRequest(const ip_addr_t *address)
{
    if (address == nullptr)
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
    request[0] = 0x23; // NTP v4, client mode
    if (pbuf_take(packet, request, sizeof(request)) != ERR_OK || udp_connect(pcb, address, NTP_PORT) != ERR_OK)
    {
        pbuf_free(packet);
        failRequest();
        return;
    }

    ntpRequestSendUs = CoreUtils::monotonic32();
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
    const uint32_t receiveUs = CoreUtils::monotonic32();

    auto *client = static_cast<NtpClient *>(arg);
    if (packet == nullptr)
    {
        if (client != nullptr)
        {
            client->ntpRequestSendUs = 0;
        }
        return;
    }

    if (client == nullptr)
    {
        pbuf_free(packet);
        return;
    }

    uint8_t response[NTP_PACKET_SIZE] = {};
    const bool responseCopied = packet->tot_len >= NTP_PACKET_SIZE &&
                                pbuf_copy_partial(packet, response, sizeof(response), 0) == sizeof(response);
    pbuf_free(packet);

    uint32_t ntpSecondsNetwork = 0;
    uint32_t ntpFractionNetwork = 0;
    std::memcpy(&ntpSecondsNetwork, response + 40, sizeof(ntpSecondsNetwork));
    std::memcpy(&ntpFractionNetwork, response + 44, sizeof(ntpFractionNetwork));
    const uint32_t ntpSeconds = lwip_ntohl(ntpSecondsNetwork);
    const uint32_t ntpFraction = lwip_ntohl(ntpFractionNetwork);
    const uint8_t version = static_cast<uint8_t>((response[0] >> 3) & 0x07);

    const bool valid = client->ntpRequestSendUs != 0 && pcb_ == client->pcb &&
                       port == NTP_PORT && responseCopied && version >= 3 && version <= 4 &&
                       (response[0] & 0x07) == 4 && // server mode
                       (response[0] >> 6) != 3 &&   // clock is synchronized
                       response[1] >= 1 && response[1] <= 15 &&
                       ntpSeconds != 0;

    if (!valid)
    {
        client->ntpRequestSendUs = 0;
        client->failRequest(Failure::REQUEST);
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
    if (roundTripMs > MAX_ROUND_TRIP_MS) {
        client->failRequest(Failure::REQUEST);
        return;
    }                            

    client->ppsCallback(static_cast<int32_t>((unixMs % 1'000ULL) * 1'000ULL));
    client->timeCallback(unixMs);
    client->ntpRequestSendUs = 0;
}

void NtpClient::failRequest(Failure failure)
{
    ntpRequestSendUs = 0;
    failureCallback(failure);
}
