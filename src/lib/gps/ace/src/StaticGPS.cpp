#include "../StaticGPS.hpp"

#include <cmath>

#include "ace/coreutils.hpp"
#include "ace/semaphoreguard.hpp"

#include "pico/time.h"

StaticGPS::StaticGPS(etl::imessage_bus &bus, const Configuration &config)
    : AbstractGnss(bus, NAME, GATAS::PinTypeMap{}, true, 0, false),
      latitude(0.0F),
      longitude(0.0F),
      altitudeMeters(0.0F),
      geoidSeparationMeters(0.0F),
      ntpClient(NtpClient::TimeCallback::create<StaticGPS, &StaticGPS::onNtpTime>(*this),
                NtpClient::PpsCallback::create<StaticGPS, &StaticGPS::onNtpPps>(*this),
                NtpClient::FailureCallback::create<StaticGPS, &StaticGPS::onNtpFailure>(*this))
{
    readConfiguration(config);
}

void StaticGPS::readConfiguration(const Configuration &config)
{
    latitude = config.floatValueByPath(0.0F, NAME, "latitude");
    longitude = config.floatValueByPath(0.0F, NAME, "longitude");
    altitudeMeters = config.floatValueByPath(0.0F, NAME, "altitude");
    GATAS::ConfigString ntpServer = config.strValueByPath("nl.pool.ntp.org", NAME, "ntpServer");

    if (!std::isfinite(latitude) || latitude < -90.0F || latitude > 90.0F)
    {
        latitude = 0.0F;
    }
    if (!std::isfinite(longitude) || longitude < -180.0F || longitude > 180.0F)
    {
        longitude = 0.0F;
    }
    if (!std::isfinite(altitudeMeters) || altitudeMeters < -1000.0F || altitudeMeters > 20000.0F)
    {
        altitudeMeters = 0.0F;
    }
    if (ntpServer.empty())
    {
        ntpServer = "nl.pool.ntp.org";
    }

    geoidSeparationMeters = static_cast<float>(CoreUtils::egmGeoidOffset(latitude, longitude));
    ntpClient.setServerName(ntpServer);
}

GATAS::PostConstruct StaticGPS::postConstruct()
{
    if (BaseModule::moduleByName(*this, "L76B") != nullptr ||
        BaseModule::moduleByName(*this, "UbloxM8N") != nullptr)
    {
        setStatus("GNSS conflict");
        return GATAS::PostConstruct::CONFIG_ERROR;
    }

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
        const uint64_t nowUs = CoreUtils::monotonic();

        if ((notification & NTP_RESULT) != 0)
        {
            applyNtpResult();
        }

        if ((notification & CONFIG_NTP_UPDATED) != 0)
        {
            applyConfigurationUpdate();
        }

        if ((notification & NETWORK_CHANGED) != 0)
        {
            ntpClient.cancel();
            if (wifiConnected)
            {
                nextNtpAttemptUs = nowUs;
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
            nextNtpAttemptUs = nowUs + NTP_RETRY_INTERVAL_US;
        }

        ntpClient.poll(nowUs);

        if (wifiConnected && !ntpClient.busy() && nowUs >= nextNtpAttemptUs)
        {
            staticStatistics.ntpRequests += 1;
            ntpClient.requestTime();
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
    coordinate.hemisphere = latitude ? (value < 0.0F ? "S" : "N") : (value < 0.0F ? "W" : "E");
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
    float currentLatitude;
    float currentLongitude;
    float currentAltitudeMeters;
    float currentGeoidSeparationMeters;
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

    const uint64_t epochMs = CoreUtils::msSinceEpoch();
    const bool valid = epochMs >= 1'000'000'000'000ULL;
    if (!valid)
    {
        staticStatistics.invalidTime += 1;
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
               << utc.tm_min
               << utc.tm_sec
               << "."
               << centiseconds;

    etl::string<6> dateText;
    etl::string_stream dateStream(dateText);
    dateStream << etl::format_spec{}.width(2).fill('0') << utc.tm_mday
               << utc.tm_mon + 1
               << (utc.tm_year + 1900) % 100;

    GATAS::NMEAString nmeaString;

    // $GPGLL
    etl::string<3> AV = (valid ? ",A" : ",V");
    {
        etl::string_stream nmeaStream(nmeaString);
        nmeaStream << "$GPGLL," << latitudeCoordinate.text << "," << latitudeCoordinate.hemisphere
                   << "," << longitudeCoordinate.text << "," << longitudeCoordinate.hemisphere
                   << "," << timeText << AV;
    }
    CoreUtils::addChecksumToNMEA(nmeaString, false);
    processNewSentenceFromTask({nmeaString.data(), nmeaString.size()});

    // $GPRMC
    nmeaString.clear();
    {
        etl::string_stream nmeaStream(nmeaString);
        nmeaStream << "$GPRMC," << timeText << AV
                   << "," << latitudeCoordinate.text << "," << latitudeCoordinate.hemisphere
                   << "," << longitudeCoordinate.text << "," << longitudeCoordinate.hemisphere
                   << ",0.000,0.00," << dateText << ",,";
    }
    CoreUtils::addChecksumToNMEA(nmeaString, false);
    processNewSentenceFromTask({nmeaString.data(), nmeaString.size()});

    // // $GPVTG
    // nmeaString = "$GPVTG,0.00,T,,M,0.000,N,0.000,K";
    // CoreUtils::addChecksumToNMEA(nmeaString, false);
    // processNewSentenceFromTask({nmeaString.data(), nmeaString.size()});

    // $GPGGA
    nmeaString.clear();
    etl::string_stream nmeaStream(nmeaString);
    nmeaStream << "$GPGGA," << timeText << "," << latitudeCoordinate.text << "," << latitudeCoordinate.hemisphere
               << "," << longitudeCoordinate.text << "," << longitudeCoordinate.hemisphere
               << "," << (valid ? "1,08" : "0,00") << ",1.0," << etl::format_spec{}.precision(1) << currentAltitudeMeters
               << GATAS::RESET_FORMAT << ",M," << etl::format_spec{}.precision(1) << currentGeoidSeparationMeters
               << GATAS::RESET_FORMAT << ",M,,";
    CoreUtils::addChecksumToNMEA(nmeaString, false);
    processNewSentenceFromTask({nmeaString.data(), nmeaString.size()});

    // $GPGSA
    nmeaString = valid
                     ? "$GPGSA,M,3,03,04,08,10,13,16,21,27,,,,,1.0,1.0,1.0"
                     : "$GPGSA,M,1,,,,,,,,,,,,,1.0,1.0,1.0";
    CoreUtils::addChecksumToNMEA(nmeaString, false);
    processNewSentenceFromTask({nmeaString.data(), nmeaString.size()});

    // $GPGSV
    nmeaString = "$GPGSV,2,1,08,03,45,111,42,04,50,272,43,08,35,046,40,10,60,151,45";
    CoreUtils::addChecksumToNMEA(nmeaString, false);
    processNewSentenceFromTask({nmeaString.data(), nmeaString.size()});

    // $GPGSV
    nmeaString = "$GPGSV,2,2,08,13,25,210,38,16,40,315,41,21,55,080,44,27,30,180,39";
    CoreUtils::addChecksumToNMEA(nmeaString, false);
    processNewSentenceFromTask({nmeaString.data(), nmeaString.size()});
}

void StaticGPS::onNtpTime(uint64_t epochMs)
{
    ntpEpochAtReceiveMs = epochMs;
    ntpReceivedAtUs = CoreUtils::monotonic();
    ntpResultPending = true;
    xTaskNotify(staticTaskHandle, NTP_RESULT, eSetBits);
}

void StaticGPS::onNtpPps(int32_t offsetUs)
{
    rtc->ppsEvent(offsetUs);
}

void StaticGPS::onNtpFailure()
{
    xTaskNotify(staticTaskHandle, NTP_FAILED, eSetBits);
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

    const uint64_t nowUs = CoreUtils::monotonic();
    const uint64_t epochNowMs = epochAtReceiveMs + (nowUs - receivedAtUs) / 1'000ULL;
    CoreUtils::setOffsetMsSinceEpoch(epochNowMs);
    staticStatistics.ntpSyncs += 1;
    nextNtpAttemptUs = nowUs + NTP_REFRESH_INTERVAL_US;
    setStatus("Configured");
}

void StaticGPS::applyConfigurationUpdate()
{
    ntpClient.cancel();
    nextNtpAttemptUs = CoreUtils::monotonic();
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

    bool ntpServerChanged = false;
    if (auto guard = SemaphoreGuard(1000, configurationMutex))
    {
        const etl::string<64> previousNtpServer(ntpClient.serverName());
        readConfiguration(msg.config);
        ntpServerChanged = ntpClient.serverName() != previousNtpServer;
    }
    else
    {
        return;
    }

    if (ntpServerChanged && staticTaskHandle != nullptr)
    {
        xTaskNotify(staticTaskHandle, CONFIG_NTP_UPDATED, eSetBits);
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
        stream << ",\"ntpServer\":\"" << ntpClient.serverName() << "\"";
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
