#include "../picortc.hpp"

void PicoRtc::getData(etl::string_stream &stream, const etl::string_view path) const
{
    (void)path;
    stream << "{";
    stream << "\"epochSet\":" << statistics.epochSet;
    stream << ",\"delayMs\":" << statistics.delayMs;
    stream << ",\"highElapseTimeErr\":" << statistics.highElapseTimeErr;
    stream << ",\"timeUs32\":" << CoreUtils::timeUs32();
    stream << ",\"secondsSinceEpoch\":" << CoreUtils::secondsSinceEpoch();
    stream << "}\n";
}

// This method is not protected with a mutex since it's called from hardware interrupt well before GATAS::UtcTimeMsg& event is end
__force_inline void PicoRtc::ppsEvent()
{
    CoreUtils::setPPS();
    lastPpstime = CoreUtils::timeUs32();
}

GATAS::PostConstruct PicoRtc::postConstruct()
{
    return GATAS::PostConstruct::OK;
}

void PicoRtc::start()
{
    getBus().subscribe(*this);
};

/**
 *   GATAS::UtcTimeMsg is normally aend every 15 seconds
 */
void PicoRtc::on_receive(const GATAS::UtcTimeMsg &msg)
{
    uint32_t elapsedUsSincePps = CoreUtils::timeUs32() - lastPpstime;

    // Must be set within a second to ensure correct timing for epoch
    if (elapsedUsSincePps > 500'000)
    {
        statistics.highElapseTimeErr += 1;
        return;
    }

    // if (GATAS_SET_PICO_RTC && msg.millisecond == 0)
    // {
    //     // Set the RTC
    //     datetime_t t =
    //     {
    //         .year  = msg.year,
    //         .month = msg.month,
    //         .day   = msg.day,
    //         .dotw  = 0, // don't use
    //         .hour  = msg.hour,
    //         .min   = msg.minute,
    //         .sec   = msg.second
    //     };
    //     rtc_set_datetime(&t);
    // }

    // Set the time of the PICO, this will use the underlaying to_us_since_boot and friends
    // Getting the time will then work when using gettimeofday(&tv, nullptr);
    struct tm timeinfo =
        {
            .tm_sec = msg.second,       // Seconds after the minute
            .tm_min = msg.minute,       // Minutes after the hour
            .tm_hour = msg.hour,        // Hours since midnight
            .tm_mday = msg.day,         // Day of the month
            .tm_mon = msg.month - 1,    // Months since January (0-based)
            .tm_year = msg.year - 1900, // Years since 1900
            .tm_wday = 0,
            .tm_yday = 0,
            .tm_isdst = 0};

    time_t secondsSinceEpoch = mktime(&timeinfo);

    // GATAS uses it's internal timekeeoing since absolute time is really not needed anywhere
    // struct timeval tv =
    // {
    //     .tv_sec = secondsSinceEpoch,
    //     .tv_usec = (suseconds_t)elapsedUsSincePps      // msg.millisecond * 1000
    // };

    // tm time = CoreUtils::localTime();
    // printf("GPS: %02d:%02d:%02d.%03d   Current Pico:%02d:%02d:%02d.%03ld \n", msg.hour, msg.minute, msg.second, elapsedUsSincePps/1000, time.tm_hour, time.tm_min, time.tm_sec, CoreUtils::msInSecond());
    //  settimeofday(&tv, nullptr);

    statistics.delayMs = CoreUtils::msInSecond();
    uint64_t msSinceEpoch = static_cast<uint64_t>(secondsSinceEpoch) * 1000 + statistics.delayMs;
    CoreUtils::setOffsetMsSinceEpoch(msSinceEpoch);
    statistics.epochSet += 1;
}

void PicoRtc::on_receive_unknown(const etl::imessage &msg)
{
    (void)msg;
}