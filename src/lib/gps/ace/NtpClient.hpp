#pragma once

#include <stdint.h>

#include "etl/delegate.h"
#include "etl/string.h"
#include "etl/string_view.h"

#include "lwip/ip_addr.h"

struct pbuf;
struct udp_pcb;

/**
 * Small asynchronous NTP client.
 *
 * Networking is handled through lwIP callbacks. The owner decides when to
 * request or cancel synchronization and supplies delegates for the resulting
 * UTC time, PPS phase, and failures. This class does not create or notify any
 * FreeRTOS tasks, queues, or timers.
 */
class NtpClient
{
public:
    using TimeCallback = etl::delegate<void(uint64_t)>;
    using PpsCallback = etl::delegate<void(int32_t)>;
    using FailureCallback = etl::delegate<void()>;

private:
    static constexpr uint16_t NTP_PORT = 123;
    static constexpr size_t NTP_PACKET_SIZE = 48;
    static constexpr uint32_t NTP_TO_UNIX_EPOCH_SECONDS = 2'208'988'800UL;
    static constexpr uint64_t REQUEST_TIMEOUT_US = 10ULL * 1'000'000ULL;

    etl::string<64> server;
    TimeCallback timeCallback;
    PpsCallback ppsCallback;
    FailureCallback failureCallback;
    udp_pcb *pcb = nullptr;
    uint64_t requestStartedUs = 0;
    uint64_t requestSentUs = 0;
    bool active = false;
    bool dnsPending = false;

    static void dnsCallback(const char *name, const ip_addr_t *address, void *arg);
    static void receiveCallback(void *arg, udp_pcb *pcb, pbuf *packet, const ip_addr_t *address, uint16_t port);

    void sendRequest(const ip_addr_t *address);
    void closePcb();
    void failRequest();

public:
    NtpClient(TimeCallback timeCallback, PpsCallback ppsCallback, FailureCallback failureCallback);
    ~NtpClient();

    NtpClient(const NtpClient &) = delete;
    NtpClient &operator=(const NtpClient &) = delete;

    void setServerName(etl::string_view serverName);
    etl::string_view serverName() const;

    bool requestTime();
    void cancel();
    void poll(uint64_t nowUs);
    bool busy() const;
};
