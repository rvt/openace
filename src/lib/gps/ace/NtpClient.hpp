#pragma once

#include <stdint.h>

#include "etl/delegate.h"
#include "etl/string.h"
#include "etl/string_view.h"

#include "lwip/ip_addr.h"

struct pbuf;
struct udp_pcb;

struct NtpTimeResult
{
    uint64_t epochMs;
    uint64_t receivedAtUs;
};

/**
 * Small asynchronous NTP client.
 *
 * Networking is handled through lwIP callbacks. The owner decides when to
 * request or cancel synchronization and supplies delegates for the resulting
 * UTC time, PPS phase, and failures. This class does not create or notify any
 * FreeRTOS tasks, queues, or timers.
 *
 * Delegates are invoked from the lwIP callback context or while the lwIP lock
 * is held. They must be non-blocking and must not call back into NtpClient.
 * The client must also outlive a pending DNS lookup because lwIP does not
 * provide a way to cancel its completion callback.
 */
class NtpClient
{
public:
    enum class Failure
    {
        REQUEST,
        ROUND_TRIP_TOO_LONG,
    };

    using ServerName = etl::string<32>;
    using TimeCallback = etl::delegate<void(const NtpTimeResult &)>;
    using PpsCallback = etl::delegate<void(int32_t)>;
    using FailureCallback = etl::delegate<void(Failure)>;

private:
    static constexpr uint16_t NTP_PORT = 123;
    static constexpr size_t NTP_PACKET_SIZE = 48;
    static constexpr uint32_t NTP_TO_UNIX_EPOCH_SECONDS = 2'208'988'800UL;
    static constexpr uint64_t MAX_ROUND_TRIP_MS = 50; // any round trip time higher than this value might indicate jitter and will result in incorrect timings

    ServerName server;
    TimeCallback timeCallback;
    PpsCallback ppsCallback;
    FailureCallback failureCallback;
    udp_pcb *pcb = nullptr;
    uint32_t ntpRequestSendUs = 0;
    bool dnsPending = false;
    bool processPending = false;

    /** Handles completion of an asynchronous DNS lookup. */
    static void dnsCallback(const char *name, const ip_addr_t *address, void *arg);

    /** Handles an incoming UDP NTP response. */
    static void receiveCallback(void *arg, udp_pcb *pcb, pbuf *packet, const ip_addr_t *address, uint16_t port);

    /** Sends an NTP request to the resolved server address. */
    void sendRequest(const ip_addr_t *address);

    /** Completes the current request with the supplied failure reason. */
    void failRequest(Failure failure = Failure::REQUEST);

public:
    /** Creates a client with callbacks for time, PPS phase, and failures. */
    NtpClient(TimeCallback timeCallback, PpsCallback ppsCallback, FailureCallback failureCallback);

    /** Cancels the client request and releases its UDP control block. */
    ~NtpClient();

    NtpClient(const NtpClient &) = delete;
    NtpClient &operator=(const NtpClient &) = delete;

    /** Sets the hostname used for subsequent NTP requests. */
    void setServerName(etl::string_view serverName);

    /** Returns the currently configured NTP hostname. */
    ServerName serverName() const;

    /** Starts an asynchronous NTP request, returning whether it was started. */
    bool requestTime();

    bool isPending() const {
        return processPending;
    }
};
