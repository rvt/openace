#include "picortc.hpp"



void PicoRtc::getData(etl::string_stream &stream, const etl::string_view path) const
{   
    (void)path;
    stream << "{";
    stream << "\"epochSet\":" << statistics.epochSet;
    stream << ",\"delayUs\":" << statistics.delayUs;
    stream << ",\"highElapseTime\":" << statistics.highElapseTime;
    stream << ",\"ppsEventsReceived\":" << statistics.ppsEventsReceived;
    stream << ",\"lastPpstime\":" << lastPpstime;
    stream << ",\"positionTs\":" << CoreUtils::getPositionTs();
    stream << "}\n";

}

// This method is not protected with a mutex since it's called from hardware interrupt well before OpenAce::GpsTime& event is end
void __time_critical_func(PicoRtc::ppsEvent)()
{
    CoreUtils::setPPS();
    lastPpstime = CoreUtils::timeUs32();
    statistics.ppsEventsReceived++;
}

OpenAce::PostConstruct PicoRtc::postConstruct()
{
    rtc_init();
    return OpenAce::PostConstruct::OK;
}

void PicoRtc::start()
{
    getBus().subscribe(*this);
};

void PicoRtc::stop()
{
    getBus().unsubscribe(*this);
};


void PicoRtc::on_receive(const OpenAce::GpsTime& msg)
{
    // printf("$RMC Time: %02d:%02d:%02d:%02d date: %04d:%02d:%02d\n",
    //     msg.hour, msg.minute, msg.second, msg.millisecond,
    //     msg.year, msg.month, msg.day);

    uint32_t elapsedUsSincePps = CoreUtils::timeUs32() - lastPpstime;


    // Don't set the time if it's more than 100ms since the last PPS and also ensures RTC
    // will be set at whole seconds only
    if (elapsedUsSincePps > 100000)
    {
        statistics.highElapseTime++;
        return;
    }

    // if (SET_PICO_RTC && msg.millisecond == 0)
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
        .tm_sec = msg.second,         // Seconds after the minute
        .tm_min = msg.minute,        // Minutes after the hour
        .tm_hour = msg.hour,         // Hours since midnight
        .tm_mday = msg.day,          // Day of the month
        .tm_mon = msg.month - 1,     // Months since January (0-based)
        .tm_year = msg.year - 1900,  // Years since 1900
        .tm_wday = 0,
        .tm_yday = 0,
        .tm_isdst = 0
    };

    time_t secondsSinceEpoch = mktime(&timeinfo);
    struct timeval tv =
    {
        .tv_sec = secondsSinceEpoch,
        .tv_usec = (suseconds_t)elapsedUsSincePps      // msg.millisecond * 1000
    };

    // tm time = CoreUtils::localTime();
    // printf("GPS: %02d:%02d:%02d.%03d   Current Pico:%02d:%02d:%02d.%03ld \n", msg.hour, msg.minute, msg.second, elapsedUsSincePps/1000, time.tm_hour, time.tm_min, time.tm_sec, CoreUtils::msInSecond());

    settimeofday(&tv, nullptr);
    elapsedUsSincePps = CoreUtils::timeUs32() - lastPpstime;
    statistics.delayUs = elapsedUsSincePps;
    CoreUtils::setOffsetMsSinceEpoch(secondsSinceEpoch*1000 + elapsedUsSincePps/1000);
    statistics.epochSet++;

}

void PicoRtc::on_receive_unknown(const etl::imessage& msg)
{
    (void)msg;
}