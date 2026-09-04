#pragma once

#include <stdint.h>

#include "AbstractGnss.hpp"
#include "NtpClient.hpp"

#include "FreeRTOS.h"
#include "semphr.h"
#include "timers.h"

#include "etl/message_router.h"
#include "etl/string.h"

#ifndef MEASURE_NTP_OFFSET
#define MEASURE_NTP_OFFSET (0)
#endif

/**
 * Generates NMEA sentences for a configured fixed position.
 *
 * The position is deliberately presented as simulated GNSS input instead of
 * injecting ownship messages directly. This keeps the normal GPS decoder,
 * time handling, message-bus routing, and DataPort/Bluetooth output paths in
 * use, so downstream modules and external devices see the same data flow as
 * they do with a hardware GNSS receiver.
 */
class StaticGPS : public AbstractGnss,
                  public etl::message_router<StaticGPS, GATAS::WifiConnectionStateMsg, GATAS::ConfigUpdatedMsg>
{
private:
    static constexpr uint32_t SEND_INTERVAL_MS = 500;
    // Retry startup synchronization promptly. Successful synchronization uses
    // the much longer refresh interval below.
    static constexpr uint64_t NTP_RETRY_INTERVAL_US = 5ULL * 1'000'000ULL;
    // Re-discipline the software PPS often enough to limit RP2040 crystal
    // drift while avoiding excessive traffic to the configured NTP server.
    static constexpr uint64_t NTP_REFRESH_INTERVAL_US = 5ULL * 60ULL * 1'000'000ULL;

    enum TaskNotification : uint32_t
    {
        SEND_SENTENCES = 1 << 0,
        NTP_RESULT = 1 << 1,
        NETWORK_CHANGED = 1 << 2,
        NTP_FAILED = 1 << 3,
        NTP_SERVER_UPDATED = 1 << 4,
        NTP_ROUND_TRIP_TOO_LONG = 1 << 5,
    };

    struct
    {
        uint32_t ntpRequests = 0;
        uint32_t ntpSyncs = 0;
        uint32_t ntpErrors = 0;
        uint32_t ntpRoundTripRejected = 0;
        uint32_t invalidTime = 0;
    } staticStatistics;

    struct Coordinate
    {
        etl::string<11> text;
        etl::string<2> hemisphere;
    };

    friend class message_router;

    float latitude;
    float longitude;
    float altitudeMeters;
    float geoidSeparationMeters;
    SemaphoreHandle_t configurationMutex = nullptr;

    TaskHandle_t staticTaskHandle = nullptr;
    TimerHandle_t sendTimerHandle = nullptr;
    RtcModule *rtc = nullptr;
    NtpClient ntpClient;
    uint64_t nextNtpAttemptUs = 0;
    uint64_t ntpEpochAtReceiveMs = 0;
    uint64_t ntpReceivedAtUs = 0;
    bool wifiConnected = false;
    bool ntpResultPending = false;

    static void taskTrampoline(void *arg);
    static void timerCallback(TimerHandle_t timer);
    static Coordinate coordinate(float value, bool latitude);

    void task();
    void readConfiguration(const Configuration &config);
    void publishSentences();
    void onNtpTime(uint64_t epochMs);
    void onNtpPps(int32_t offsetUs);
    void onNtpFailure(NtpClient::Failure failure);
    void applyNtpResult();
    void applyConfigurationUpdate();

    void on_receive(const GATAS::WifiConnectionStateMsg &msg);
    void on_receive(const GATAS::ConfigUpdatedMsg &msg);
    void on_receive_unknown(const etl::imessage &msg);

    bool configureGnss() override;

public:
    static constexpr const etl::string_view NAME = "StaticGPS";

    StaticGPS(etl::imessage_bus &bus, const Configuration &config);
    ~StaticGPS() override = default;

    GATAS::PostConstruct postConstruct() override;
    void start() override;
    void getData(etl::string_stream &stream, const etl::string_view path) const override;
};
