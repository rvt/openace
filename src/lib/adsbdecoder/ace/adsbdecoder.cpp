#include <stdio.h>

#include "ace/semaphoreguard.hpp"
#include "adsbdecoder.hpp"

OpenAce::PostConstruct ADSBDecoder::postConstruct()
{
    mutex = xSemaphoreCreateMutex();
    if (mutex == nullptr)
    {
        return OpenAce::PostConstruct::MUTEX_ERROR;
    }
    state.fix_errors = false;
    mode_s_init(&state);
    ignoredAirplanes.clear();
    adsbDataCollector.clear();
    return OpenAce::PostConstruct::OK;
}

void ADSBDecoder::start()
{
    getBus().subscribe(*this);
};

void ADSBDecoder::stop()
{
    getBus().unsubscribe(*this);
};

void ADSBDecoder::on_receive(const OpenAce::OwnshipPositionMsg &msg)
{
    ownshipPosition = msg.position;
}

void ADSBDecoder::getConfiguration(const Configuration &config)
{
    filterAbove = config.valueByPath(true, NAME, "filterAbove");
    filterBelow = config.valueByPath(true, NAME, "filterBelow");
}

void ADSBDecoder::on_receive(const OpenAce::ConfigUpdatedMsg &msg)
{
    if (msg.moduleName == ADSBDecoder::NAME)
    {
        getConfiguration(msg.config);
    }
}

void ADSBDecoder::receiveBinary(const uint8_t *data, uint8_t length)
{
    processAdsbData(data, length);
}

void ADSBDecoder::on_receive(const OpenAce::ADSBMessageBin &msg)
{
    processAdsbData(msg.data, msg.length);
}

void ADSBDecoder::processAdsbData(const uint8_t *data, uint8_t length)
{
    (void)length;
    mode_s_msg mm;
    // Two phase decoder to first decode the address.. when not in ignoredAirplanes continue decoding
    mode_s_decode_phase1(&state, &mm, data);

    if (mm.msgtype != 17)
    {
        return;
    }

    if (!mm.crcok)
    {
        statistics.crcErrors++;
        return;
    }

    if (auto guard = SemaphoreGuard<10>(mutex))
    {
        // printf("Processing  a:%06lX \n", mm.aa);
        auto usTime = CoreUtils::timeUs32Raw();
        if (ignoredAirplanes.ifContainsThenUpdate(mm.aa, usTime))
        {
            statistics.totalMsgIgnored++;
            return;
        }

        mode_s_decode_phase2(&state, &mm);

        if (!adsbDataCollector.start(mm.aa, usTime))
        {
            statistics.knownAircraftFull++;
            return;
        }

        /**
         * https://mode-s.org/decode/content/ads-b/1-basics.html
         */
        adsbDataCollector.updateAirborne(true);
        if (mm.metype >= 1 && mm.metype <= 4)
        {
            adsbDataCollector.updateCallsign(mm.flight, mm.aircraft_type);
        }
        else if ((mm.metype >= 9 && mm.metype <= 18) || (mm.metype >= 20 && mm.metype <= 22))
        {
            if (mm.fflag)
            {
                adsbDataCollector.updateRawOdd(mm.raw_latitude, mm.raw_longitude);
            }
            else
            {
                adsbDataCollector.updateRawEven(mm.raw_latitude, mm.raw_longitude);
            }

            // THis might temporary create a incorrect offset, but should be corrected pretty quickly die to how ADSB sends different information in different packages
            // In addition this is also not the true altitude
            int32_t altitude = (mm.unit == MODE_S_UNIT_METERS ? mm.altitude : mm.altitude * FT_TO_M);

            if (mm.metype >= 20 && mm.metype <= 22) // GPS Altitude so far never seen this
            {
                adsbDataCollector.updateGnssAltitude(altitude); 
            }
            else if (mm.metype >= 9 && mm.metype <= 18) // Barometric Altitude
            {
                auto &current = adsbDataCollector.current();
                adsbDataCollector.updateAltitude(altitude + CoreUtils::egmGeoidOffset(current.lat, current.lon));
            }
        }
        else if (mm.metype == 19 && mm.mesub >= 1 && mm.mesub <= 4)
        {
            if (mm.mesub == 1 || mm.mesub == 2)
            {
                // printf("%06lX Ellipsoid:%ldm\n", mm.aa, baro_gnss_diff);
                adsbDataCollector.updateVelocityHeadingBaroDiff(mm.velocity, mm.vert_rate_sign ? -mm.vert_rate : mm.vert_rate, mm.heading, mm.head * FT_TO_M); // mm.head is always in feet
            }
            else if (mm.mesub == 3 || mm.mesub == 4)
            {
                if (mm.heading_is_valid)
                {
                    adsbDataCollector.updateHeading(mm.heading);
                }
            }
        }

        // Used to take specific performance measurements
        // auto usDuration = CoreUtils::timeUs32Raw();
        // const static int EVERY = 250;
        // static int count = 0;
        // static uint32_t duration = 0;
        // duration = duration + (usDuration - usSinceBoot);
        // count++;
        // if (count % EVERY == 0)
        // {
        //     puts("\033[;H");
        //     printf("\n counter:%d msgCount:%d took:%.2fus\n", count, msgCount, (float)duration / EVERY);
        //     duration = 0;
        //     adsbDataCollector.dump();
        // }
        if (adsbDataCollector.positionUpdatedAndValid())
        {
            statistics.totalMsgReceived++;
            auto &current = adsbDataCollector.current();

            auto fromOwn = CoreUtils::getDistanceRelNorthRelEastInt(ownshipPosition.lat, ownshipPosition.lon, current.lat, current.lon);

            // Altitude filtering + Radius filtering
            // to prevent aircraft taking up resources that are not a factor.
            if (outOfAltitudeRange(current.altitudeHAE) || fromOwn.distance > filterRadius)
            {
                statistics.totalMsgIgnored++;
                // printf("Ignored  t:%06ld a:%06lX gnsAlt:%ldm gnsAlt:%0.2fft distance:%ld\n", usTime / 1'000'000, current.icao, current.altitudeHAE, current.altitudeHAE * M_TO_FT, fromOwn.distance);
                if (!ignoredAirplanes.insert(current.icao, usTime))
                {
                    statistics.ignoredAircraftFull++;
                }

                current.evict = true;
                return;
            }

            float vertical_rate = current.vert_rate;
            if (vertical_rate > 0)
            {
                // https://mode-s.org/1090mhz/content/ads-b/5-airborne-velocity.html
                vertical_rate = (current.vert_rate - 1) * 64.f * FTPMIN_TO_MS;
            }

            // printf("Received  t:%06ld a:%06lX gnsAlt:%ldm gnsAlt:%0.2fft\n", usTime / 1'000'000, current.icao, current.altitudeHAE, current.altitudeHAE * M_TO_FT);
            getBus().receive(OpenAce::AircraftPositionMsg{
                {usTime - ADSBDECODER_US_DELAY_SERIAL_AND_OVERHEAD,
                 current.callSign,
                 current.icao,
                 OpenAce::AddressType::ICAO,
                 OpenAce::DataSource::ADSB,
                 OpenAce::AircraftCategory::Unknown, // ADSB does not have type
                 false,                              // ADSB does not have privacy
                 false,                              // No privacy
                 current.airborne,
                 current.lat,
                 current.lon,
                 current.altitudeHAE > INT16_MAX ? INT16_MAX : static_cast<int16_t>(current.altitudeHAE),
                 vertical_rate, // https://mode-s.org/decode/content/ads-b/5-airborne-velocity.html,
                 (float)current.velocity * KN_TO_MS,
                 static_cast<int16_t>(current.heading),
                 0.0f,
                 fromOwn.distance,
                 fromOwn.relNorth,
                 fromOwn.relEast,
                 fromOwn.bearing}});
        }
    } else {
        statistics.msgMissed++;
    }
}

void ADSBDecoder::on_receive(const OpenAce::IdleMsg &msg)
{
    (void)msg;
    if (auto guard = SemaphoreGuard<10>(mutex))
    {
        auto usTime = CoreUtils::timeUs32Raw();
        ignoredAirplanes.evictOldEntries(usTime);
        adsbDataCollector.evictOldEntries(usTime);
    }
}

void ADSBDecoder::on_receive(const OpenAce::AdapativeRadiusMsg &msg)
{
    filterRadius = msg.radius;
}

void ADSBDecoder::getData(etl::string_stream &stream, const etl::string_view path) const
{
    (void)path;
    stream << "{";
    stream << "\"crcErrors\":" << statistics.crcErrors;
    stream << ",\"adsbDataCollectorSize\":" << adsbDataCollector.size();
    stream << ",\"adsbDataCollectorFull\":" << statistics.knownAircraftFull;
    stream << ",\"ignoredAircraftSize\":" << ignoredAirplanes.size();
    stream << ",\"ignoredAircraftFull\":" << statistics.ignoredAircraftFull;
    stream << ",\"totalMsgReceived\":" << statistics.totalMsgReceived;
    stream << ",\"totalMsgIgnored\":" << statistics.totalMsgIgnored;
    stream << ",\"msgMissed\":" << statistics.msgMissed;    
    stream << ",\"filterRadius\":" << filterRadius;
    stream << "}\n";
}
