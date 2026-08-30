#include "../StaticGPS.hpp"

#include <cmath>
#include <cstring>

#include "ace/coreutils.hpp"
#include "ace/lwiplock.hpp"
#include "ace/semaphoreguard.hpp"

#include "lwip/def.h"
#include "lwip/dns.h"
#include "lwip/pbuf.h"
#include "lwip/udp.h"
#include "pico/time.h"

namespace
{
    constexpr uint16_t NTP_PORT = 123;
    constexpr size_t NTP_PACKET_SIZE = 48;
    constexpr uint32_t NTP_TO_UNIX_EPOCH_SECONDS = 2'208'988'800UL;
}

StaticGPS::StaticGPS(etl::imessage_bus &bus, float latitude_, float longitude_, float altitudeMeters_, const etl::string_view ntpServer_)
    : AbstractGnss(bus, NAME, GATAS::PinTypeMap{}, true, 0, false),
      latitude(latitude_),
      longitude(longitude_),
      altitudeMeters(altitudeMeters_),
      geoidSeparationMeters(static_cast<float>(CoreUtils::egmGeoidOffset(latitude_, longitude_))),
      ntpServer(ntpServer_)
{
}

StaticGPS::StaticGPS(etl::imessage_bus &bus, const Configuration &config)
    : StaticGPS(bus,
                config.floatValueByPath(0.0F, NAME, "latitude"),
                config.floatValueByPath(0.0F, NAME, "longitude"),
                config.floatValueByPath(0.0F, NAME, "altitude"),
                config.strValueByPath("pool.ntp.org", NAME, "ntpServer"))
{
}

GATAS::PostConstruct StaticGPS::postConstruct()
{
    const GATAS::PostConstruct result = AbstractGnss::postConstruct();
    if (result != GATAS::PostConstruct::OK)
    {
        return result;
    }

    configurationMutex = xSemaphoreCreateMutex();
    if (configurationMutex == nullptr)
    {
        return GATAS::PostConstruct::MUTEX_ERROR;
    }
    GATAS_REGISTER_MUTEX(configurationMutex, "StaticGPS_configurationMutex");

    if (ntpServer.empty())
    {
        setStatus("No NTP Server");
        return GATAS::PostConstruct::CONFIG_ERROR;
    }

    rtc = static_cast<RtcModule *>(moduleByName(*this, RtcModule::NAME));
    if (rtc == nullptr)
    {
        setStatus("No RTC");
        return GATAS::PostConstruct::DEP_NOT_FOUND;
    }

    setStatus("Waiting WiFi");
    return GATAS::PostConstruct::OK;
}

void StaticGPS::start()
{
    AbstractGnss::start();

    if (xTaskCreate(taskTrampoline, NAME.cbegin(), configMINIMAL_STACK_SIZE + 1024, this, tskIDLE_PRIORITY + 3, &staticTaskHandle) != pdPASS)
    {
        setStatus("Task error");
        return;
    }

    sendTimerHandle = xTimerCreate(NAME.cbegin(), TASK_DELAY_MS(SEND_INTERVAL_MS), pdTRUE, this, timerCallback);
    if (sendTimerHandle == nullptr || xTimerStart(sendTimerHandle, 0) != pdPASS)
    {
        setStatus("Timer error");
        return;
    }

    getBus().subscribe(*this);
}

void StaticGPS::taskTrampoline(void *arg)
{
    static_cast<StaticGPS *>(arg)->task();
}

void StaticGPS::timerCallback(TimerHandle_t timer)
{
    auto *staticGps = static_cast<StaticGPS *>(pvTimerGetTimerID(timer));
    if (staticGps != nullptr && staticGps->staticTaskHandle != nullptr)
    {
        xTaskNotify(staticGps->staticTaskHandle, SEND_SENTENCES, eSetBits);
    }
}

void StaticGPS::task()
{
    while (true)
    {
        uint32_t notification = 0;
        xTaskNotifyWait(pdFALSE, ULONG_MAX, &notification, portMAX_DELAY);

        if ((notification & NTP_RESULT) != 0)
        {
            applyNtpResult();
        }

        if ((notification & CONFIG_UPDATED) != 0)
        {
            applyConfigurationUpdate();
        }

        if ((notification & NETWORK_CHANGED) != 0)
        {
            cancelNtpRequest();
            if (wifiConnected)
            {
                nextNtpAttemptUs = CoreUtils::timeUs64();
                setStatus("Waiting NTP");
            }
            else
            {
                setStatus("Waiting WiFi");
            }
        }

        if ((notification & NTP_FAILED) != 0)
        {
            staticStatistics.ntpErrors += 1;
            nextNtpAttemptUs = CoreUtils::timeUs64() + NTP_RETRY_INTERVAL_US;
        }

        const uint64_t nowUs = CoreUtils::timeUs64();
        if (ntpRequestActive && nowUs - ntpRequestStartedUs >= NTP_TIMEOUT_US)
        {
            cancelNtpRequest();
            staticStatistics.ntpErrors += 1;
            nextNtpAttemptUs = nowUs + NTP_RETRY_INTERVAL_US;
        }

        if (wifiConnected && !ntpRequestActive && nowUs >= nextNtpAttemptUs)
        {
            beginNtpRequest();
        }

        if ((notification & SEND_SENTENCES) != 0)
        {
            publishSentences();
        }
    }
}

StaticGPS::Coordinate StaticGPS::coordinate(float value, bool latitude)
{
    Coordinate coordinate;
    coordinate.hemisphere = latitude ? (value < 0.0F ? 'S' : 'N') : (value < 0.0F ? 'W' : 'E');
    const float absolute = std::fabs(value);
    int degrees = static_cast<int>(absolute);
    float minutes = std::round((absolute - static_cast<float>(degrees)) * 60.0F * 100000.0F) / 100000.0F;
    if (minutes >= 60.0F)
    {
        minutes = 0.0F;
        degrees += 1;
    }

    etl::string_stream stream(coordinate.text);
    stream << etl::format_spec{}.width(latitude ? 2 : 3).fill('0') << degrees
           << etl::format_spec{}.precision(5).width(8).fill('0') << minutes;
    return coordinate;
}

void StaticGPS::publishSentences()
{
    float currentLatitude = 0.0F;
    float currentLongitude = 0.0F;
    float currentAltitudeMeters = 0.0F;
    float currentGeoidSeparationMeters = 0.0F;
    if (auto guard = SemaphoreGuard(1000, configurationMutex))
    {
        currentLatitude = latitude;
        currentLongitude = longitude;
        currentAltitudeMeters = altitudeMeters;
        currentGeoidSeparationMeters = geoidSeparationMeters;
    }
    else
    {
        return;
    }

    if (!std::isfinite(currentLatitude) || currentLatitude < -90.0F || currentLatitude > 90.0F ||
        !std::isfinite(currentLongitude) || currentLongitude < -180.0F || currentLongitude > 180.0F ||
        !std::isfinite(currentAltitudeMeters) || currentAltitudeMeters < -1000.0F || currentAltitudeMeters > 20000.0F)
    {
        return;
    }

    // Until NTP or another source has established epoch time, emitting 1970
    // would falsely claim a valid GNSS time and date.
    const uint64_t epochMs = CoreUtils::msSinceEpoch();
    if (epochMs < 1'000'000'000'000ULL)
    {
        staticStatistics.invalidTime += 1;
        return;
    }

    const Coordinate latitudeCoordinate = coordinate(currentLatitude, true);
    const Coordinate longitudeCoordinate = coordinate(currentLongitude, false);

    const time_t seconds = static_cast<time_t>(epochMs / 1000);
    struct tm utc = {};
    if (gmtime_r(&seconds, &utc) == nullptr)
    {
        return;
    }

    etl::string<9> timeText;
    etl::string_stream timeStream(timeText);
    const uint32_t centiseconds = static_cast<uint32_t>((epochMs % 1000) / 10);
    timeStream << etl::format_spec{}.width(2).fill('0') << utc.tm_hour
               << etl::format_spec{}.width(2).fill('0') << utc.tm_min
               << etl::format_spec{}.width(2).fill('0') << utc.tm_sec
               << GATAS::RESET_FORMAT << "."
               << etl::format_spec{}.width(2).fill('0') << centiseconds;

    etl::string<6> dateText;
    etl::string_stream dateStream(dateText);
    dateStream << etl::format_spec{}.width(2).fill('0') << utc.tm_mday
               << etl::format_spec{}.width(2).fill('0') << utc.tm_mon + 1
               << etl::format_spec{}.width(2).fill('0') << (utc.tm_year + 1900) % 100;

    GATAS::NMEAString nmeaString;

    // $GPGLL
    {
        etl::string_stream nmeaStream(nmeaString);
        nmeaStream << "$GPGLL," << latitudeCoordinate.text << "," << etl::string_view(&latitudeCoordinate.hemisphere, 1)
                   << "," << longitudeCoordinate.text << "," << etl::string_view(&longitudeCoordinate.hemisphere, 1)
                   << "," << timeText << ",A";
    }
    CoreUtils::addChecksumToNMEA(nmeaString, false);
    processNewSentence({nmeaString.data(), nmeaString.size()});

    // $GPRMC
    nmeaString.clear();
    {
        etl::string_stream nmeaStream(nmeaString);
        nmeaStream << "$GPRMC," << timeText << ",A," << latitudeCoordinate.text << "," << etl::string_view(&latitudeCoordinate.hemisphere, 1)
                   << "," << longitudeCoordinate.text << "," << etl::string_view(&longitudeCoordinate.hemisphere, 1)
                   << ",0.000,0.00," << dateText << ",,";
    }
    CoreUtils::addChecksumToNMEA(nmeaString, false);
    processNewSentence({nmeaString.data(), nmeaString.size()});

    // // $GPVTG
    // nmeaString = "$GPVTG,0.00,T,,M,0.000,N,0.000,K";
    // CoreUtils::addChecksumToNMEA(nmeaString, false);
    // processNewSentence({nmeaString.data(), nmeaString.size()});

    // $GPGGA
    nmeaString.clear();
    etl::string_stream nmeaStream(nmeaString);
    nmeaStream << "$GPGGA," << timeText << "," << latitudeCoordinate.text << "," << etl::string_view(&latitudeCoordinate.hemisphere, 1)
               << "," << longitudeCoordinate.text << "," << etl::string_view(&longitudeCoordinate.hemisphere, 1)
               << ",1,08,1.0," << etl::format_spec{}.precision(1) << currentAltitudeMeters
               << GATAS::RESET_FORMAT << ",M," << etl::format_spec{}.precision(1) << currentGeoidSeparationMeters
               << GATAS::RESET_FORMAT << ",M,,";
    CoreUtils::addChecksumToNMEA(nmeaString, false);
    processNewSentence({nmeaString.data(), nmeaString.size()});

    // $GPGSA
    nmeaString = "$GPGSA,A,3,03,04,08,10,13,16,21,27,,,,,1.0,1.0,1.0";
    CoreUtils::addChecksumToNMEA(nmeaString, false);
    processNewSentence({nmeaString.data(), nmeaString.size()});

    // $GPGSV
    nmeaString = "$GPGSV,2,1,08,03,45,111,42,04,50,272,43,08,35,046,40,10,60,151,45";
    CoreUtils::addChecksumToNMEA(nmeaString, false);
    processNewSentence({nmeaString.data(), nmeaString.size()});

    // $GPGSV
    nmeaString = "$GPGSV,2,2,08,13,25,210,38,16,40,315,41,21,55,080,44,27,30,180,39";
    CoreUtils::addChecksumToNMEA(nmeaString, false);
    processNewSentence({nmeaString.data(), nmeaString.size()});
}

void StaticGPS::beginNtpRequest()
{
    GATAS::ConfigString currentNtpServer;
    if (auto guard = SemaphoreGuard(1000, configurationMutex))
    {
        currentNtpServer = ntpServer;
    }
    else
    {
        return;
    }

    cancelNtpRequest();

    LwipLock lock;
    ntpPcb = udp_new_ip_type(IPADDR_TYPE_ANY);
    if (ntpPcb == nullptr)
    {
        staticStatistics.ntpErrors += 1;
        nextNtpAttemptUs = CoreUtils::timeUs64() + NTP_RETRY_INTERVAL_US;
        return;
    }

    udp_recv(ntpPcb, ntpReceiveCallback, this);
    ntpRequestActive = true;
    ntpRequestStartedUs = CoreUtils::timeUs64();
    staticStatistics.ntpRequests += 1;

    ip_addr_t address;
    const err_t error = dns_gethostbyname(currentNtpServer.c_str(), &address, dnsCallback, this);
    if (error == ERR_OK)
    {
        sendNtpRequest(&address);
    }
    else if (error != ERR_INPROGRESS)
    {
        udp_remove(ntpPcb);
        ntpPcb = nullptr;
        ntpRequestActive = false;
        staticStatistics.ntpErrors += 1;
        nextNtpAttemptUs = CoreUtils::timeUs64() + NTP_RETRY_INTERVAL_US;
    }
}

void StaticGPS::dnsCallback(const char *name, const ip_addr_t *address, void *arg)
{
    (void)name;
    auto *staticGps = static_cast<StaticGPS *>(arg);
    if (staticGps == nullptr || !staticGps->ntpRequestActive)
    {
        return;
    }

    if (address == nullptr)
    {
        staticGps->ntpRequestActive = false;
        if (staticGps->ntpPcb != nullptr)
        {
            udp_remove(staticGps->ntpPcb);
            staticGps->ntpPcb = nullptr;
        }
        xTaskNotify(staticGps->staticTaskHandle, NTP_FAILED, eSetBits);
        return;
    }

    staticGps->sendNtpRequest(address);
}

void StaticGPS::sendNtpRequest(const ip_addr_t *address)
{
    if (!ntpRequestActive || ntpPcb == nullptr || address == nullptr)
    {
        return;
    }

    pbuf *packet = pbuf_alloc(PBUF_TRANSPORT, NTP_PACKET_SIZE, PBUF_RAM);
    if (packet == nullptr)
    {
        return;
    }

    uint8_t request[NTP_PACKET_SIZE] = {};
    request[0] = 0x1B; // NTP v3, client mode
    if (pbuf_take(packet, request, sizeof(request)) != ERR_OK)
    {
        pbuf_free(packet);
        return;
    }
    if (udp_connect(ntpPcb, address, NTP_PORT) == ERR_OK)
    {
        udp_send(ntpPcb, packet);
    }
    pbuf_free(packet);
}

void StaticGPS::ntpReceiveCallback(void *arg, udp_pcb *pcb, pbuf *packet, const ip_addr_t *address, uint16_t port)
{
    (void)address;
    auto *staticGps = static_cast<StaticGPS *>(arg);
    if (staticGps == nullptr || packet == nullptr)
    {
        if (packet != nullptr)
        {
            pbuf_free(packet);
        }
        return;
    }

    uint8_t response[NTP_PACKET_SIZE] = {};
    const bool valid = staticGps->ntpRequestActive && port == NTP_PORT &&
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

    const uint64_t receiveUs = CoreUtils::timeUs64();
    const uint64_t roundTripMs = (receiveUs - staticGps->ntpRequestStartedUs) / 1'000ULL;
    const uint64_t unixMs = unixSeconds * 1'000ULL +
                            ((static_cast<uint64_t>(ntpFraction) * 1'000ULL) >> 32) +
                            roundTripMs / 2;

    staticGps->ntpEpochAtReceiveMs = unixMs;
    staticGps->ntpReceivedAtUs = receiveUs;
    staticGps->ntpResultPending = true;

    staticGps->ntpRequestActive = false;
    if (pcb != nullptr)
    {
        udp_remove(pcb);
        staticGps->ntpPcb = nullptr;
    }
    xTaskNotify(staticGps->staticTaskHandle, NTP_RESULT, eSetBits);
}

void StaticGPS::cancelNtpRequest()
{
    LwipLock lock;
    if (ntpPcb != nullptr)
    {
        udp_remove(ntpPcb);
        ntpPcb = nullptr;
    }
    ntpRequestActive = false;
}

void StaticGPS::applyNtpResult()
{
    uint64_t epochAtReceiveMs = 0;
    uint64_t receivedAtUs = 0;
    if (ntpResultPending)
    {
        epochAtReceiveMs = ntpEpochAtReceiveMs;
        receivedAtUs = ntpReceivedAtUs;
        ntpResultPending = false;
    }

    if (epochAtReceiveMs == 0)
    {
        return;
    }

    const uint64_t nowUs = CoreUtils::timeUs64();
    const uint64_t epochNowMs = epochAtReceiveMs + (nowUs - receivedAtUs) / 1'000ULL;
    // NTP gives the current fractional UTC second. Feeding that phase through
    // RtcModule keeps PicoRtc's PPS state and CoreUtils' software PPS aligned.
    rtc->ppsEvent(static_cast<int32_t>((epochNowMs % 1'000ULL) * 1'000ULL));
    CoreUtils::setOffsetMsSinceEpoch(epochNowMs);
    staticStatistics.ntpSyncs += 1;
    nextNtpAttemptUs = nowUs + NTP_REFRESH_INTERVAL_US;
    setStatus("Configured");
}

void StaticGPS::applyConfigurationUpdate()
{
    cancelNtpRequest();
    nextNtpAttemptUs = CoreUtils::timeUs64();
    setStatus(wifiConnected ? "Waiting NTP" : "Waiting WiFi");
}

void StaticGPS::on_receive(const GATAS::WifiConnectionStateMsg &msg)
{
    wifiConnected = msg.wifiMode == GATAS::WifiMode::CLIENT;

    if (staticTaskHandle != nullptr)
    {
        xTaskNotify(staticTaskHandle, NETWORK_CHANGED, eSetBits);
    }
}

void StaticGPS::on_receive(const GATAS::ConfigUpdatedMsg &msg)
{
    if (msg.moduleName != StaticGPS::NAME && msg.moduleName != Configuration::NAME)
    {
        return;
    }

    const float updatedLatitude = msg.config.floatValueByPath(0.0F, NAME, "latitude");
    const float updatedLongitude = msg.config.floatValueByPath(0.0F, NAME, "longitude");
    const float updatedAltitudeMeters = msg.config.floatValueByPath(0.0F, NAME, "altitude");
    const GATAS::ConfigString updatedNtpServer = msg.config.strValueByPath("pool.ntp.org", NAME, "ntpServer");

    if (!std::isfinite(updatedLatitude) || updatedLatitude < -90.0F || updatedLatitude > 90.0F ||
        !std::isfinite(updatedLongitude) || updatedLongitude < -180.0F || updatedLongitude > 180.0F ||
        !std::isfinite(updatedAltitudeMeters) || updatedAltitudeMeters < -1000.0F || updatedAltitudeMeters > 20000.0F ||
        updatedNtpServer.empty())
    {
        return;
    }

    bool ntpServerChanged = false;
    if (auto guard = SemaphoreGuard(1000, configurationMutex))
    {
        ntpServerChanged = ntpServer != updatedNtpServer;
        latitude = updatedLatitude;
        longitude = updatedLongitude;
        altitudeMeters = updatedAltitudeMeters;
        geoidSeparationMeters = static_cast<float>(CoreUtils::egmGeoidOffset(latitude, longitude));
        ntpServer = updatedNtpServer;
    }
    else
    {
        return;
    }

    if (ntpServerChanged && staticTaskHandle != nullptr)
    {
        xTaskNotify(staticTaskHandle, CONFIG_UPDATED, eSetBits);
    }
}

void StaticGPS::on_receive_unknown(const etl::imessage &msg)
{
    (void)msg;
}

bool StaticGPS::configureGnss()
{
    return true;
}

void StaticGPS::getData(etl::string_stream &stream, const etl::string_view path) const
{
    (void)path;
    stream << "{";
    if (auto guard = SemaphoreGuard(1000, configurationMutex))
    {
        stream << "\"latitude\":" << etl::format_spec{}.precision(7) << latitude;
        stream << ",\"longitude\":" << longitude;
        stream << ",\"altitude:m\":" << altitudeMeters;
        stream << ",\"geoidSeparation:m\":" << geoidSeparationMeters;
        stream << ",\"ntpServer\":\"" << ntpServer << "\"";
    }
    else
    {
        stream << "\"configReadError:err\":1";
    }
    stream << ",\"status\":\"" << statistics.status << "\"";
    stream << ",\"totalReceived:k\":" << statistics.totalReceived;
    stream << ",\"ntpRequests:k\":" << staticStatistics.ntpRequests;
    stream << ",\"ntpSyncs:k\":" << staticStatistics.ntpSyncs;
    stream << ",\"ntpErrors:err\":" << staticStatistics.ntpErrors;
    stream << ",\"waitingForTime:k\":" << staticStatistics.invalidTime;
    stream << "}";
}
