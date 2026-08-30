#include <catch2/catch_test_macros.hpp>

#include <string>

#include "minmea.h"

namespace
{
    bool validChecksum(const GATAS::NMEAString &sentence)
    {
        const auto star = sentence.find('*');
        if (sentence.empty() || sentence.front() != '$' || star == GATAS::NMEAString::npos || star + 2 >= sentence.size())
        {
            return false;
        }

        uint8_t checksum = 0;
        for (size_t i = 1; i < star; i++)
        {
            checksum ^= static_cast<uint8_t>(sentence[i]);
        }

        auto nibble = [](char value) -> uint8_t
        {
            if (value >= '0' && value <= '9')
            {
                return static_cast<uint8_t>(value - '0');
            }
            if (value >= 'A' && value <= 'F')
            {
                return static_cast<uint8_t>(value - 'A' + 10);
            }
            return UINT8_MAX;
        };

        return checksum == static_cast<uint8_t>((nibble(sentence[star + 1]) << 4) | nibble(sentence[star + 2]));
    }
}

TEST_CASE("StaticGPS assembles a stationary north-facing NMEA sample")
{
    constexpr uint64_t epochMs = 1'787'769'125'500ULL; // 2026-08-26T18:32:05.500Z
    auto sentences = StaticGPSNmea::create(52.8925725F, 4.7362311667F, 12.3F, epochMs);

    REQUIRE(sentences.has_value());
    REQUIRE(sentences->gll.starts_with("$GPGLL,5253.55446,N,00444.17388,E,183205.50,A*"));
    REQUIRE(sentences->rmc.starts_with("$GPRMC,183205.50,A,5253.55446,N,00444.17388,E,0.000,0.00,260826,,*"));
    REQUIRE(sentences->vtg.starts_with("$GPVTG,0.00,T,,M,0.000,N,0.000,K*"));
    REQUIRE(sentences->gga.starts_with("$GPGGA,183205.50,5253.55446,N,00444.17388,E,1,08,1.0,12.3,M,0.0,M,,*"));
    REQUIRE(sentences->gsa.starts_with("$GPGSA,A,3,"));

    REQUIRE(validChecksum(sentences->gll));
    REQUIRE(validChecksum(sentences->rmc));
    REQUIRE(validChecksum(sentences->vtg));
    REQUIRE(validChecksum(sentences->gga));
    REQUIRE(validChecksum(sentences->gsa));

    minmea_sentence_rmc rmc = {};
    minmea_sentence_gga gga = {};
    minmea_sentence_gsa gsa = {};
    REQUIRE(minmea_parse_rmc(&rmc, sentences->rmc.c_str()));
    REQUIRE(minmea_parse_gga(&gga, sentences->gga.c_str()));
    REQUIRE(minmea_parse_gsa(&gsa, sentences->gsa.c_str()));
    REQUIRE(rmc.valid);
    REQUIRE(rmc.speed.value == 0);
    REQUIRE(rmc.course.value == 0);
    REQUIRE(gga.fix_quality == 1);
    REQUIRE(gsa.fix_type == 3);
}

TEST_CASE("StaticGPS handles hemispheres and rejects invalid configuration")
{
    auto sentences = StaticGPSNmea::create(-33.5F, -70.75F, 500.0F, 1'777'139'525'000ULL);
    REQUIRE(sentences.has_value());
    REQUIRE(sentences->gll.starts_with("$GPGLL,3330.00000,S,07045.00000,W,"));

    REQUIRE_FALSE(StaticGPSNmea::create(90.1F, 0.0F, 0.0F, 1'777'139'525'000ULL).has_value());
    REQUIRE_FALSE(StaticGPSNmea::create(0.0F, -180.1F, 0.0F, 1'777'139'525'000ULL).has_value());
    REQUIRE_FALSE(StaticGPSNmea::create(0.0F, 0.0F, 20000.1F, 1'777'139'525'000ULL).has_value());
}
