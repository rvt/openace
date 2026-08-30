#include <catch2/catch_test_macros.hpp>

#include "ace/constants.hpp"
#include "ace/coreutils.hpp"

#include "etl/array.h"

#include "minmea.h"

namespace
{
    void addChecksum(GATAS::NMEAString &sentence)
    {
        CoreUtils::addChecksumToNMEA(sentence, false);
    }

    bool validSentence(const GATAS::NMEAString &sentence)
    {
        return !sentence.ends_with("\r\n") && CoreUtils::validateNMEAChecksum(sentence);
    }
}

TEST_CASE("StaticGPS NMEA sentences are accepted by the GPS decoder parser")
{
    etl::array<GATAS::NMEAString, 6> sentences{
        "$GPGLL,5253.55446,N,00444.17388,E,183205.50,A",
        "$GPRMC,183205.50,A,5253.55446,N,00444.17388,E,0.000,0.00,260826,,",
        "$GPGGA,183205.50,5253.55446,N,00444.17388,E,1,08,1.0,12.3,M,42.0,M,,",
        "$GPGSA,A,3,03,04,08,10,13,16,21,27,,,,,1.0,1.0,1.0",
        "$GPGSV,2,1,08,03,45,111,42,04,50,272,43,08,35,046,40,10,60,151,45",
        "$GPGSV,2,2,08,13,25,210,38,16,40,315,41,21,55,080,44,27,30,180,39"};

    for (auto &sentence : sentences)
    {
        addChecksum(sentence);
        REQUIRE(validSentence(sentence));
    }

    minmea_sentence_gll gll = {};
    minmea_sentence_rmc rmc = {};
    minmea_sentence_gga gga = {};
    minmea_sentence_gsa gsa = {};
    minmea_sentence_gsv gsvPage1 = {};
    minmea_sentence_gsv gsvPage2 = {};

    REQUIRE(minmea_parse_gll(&gll, sentences[0].c_str()));
    REQUIRE(minmea_parse_rmc(&rmc, sentences[1].c_str()));
    REQUIRE(minmea_parse_gga(&gga, sentences[2].c_str()));
    REQUIRE(minmea_parse_gsa(&gsa, sentences[3].c_str()));
    REQUIRE(minmea_parse_gsv(&gsvPage1, sentences[4].c_str()));
    REQUIRE(minmea_parse_gsv(&gsvPage2, sentences[5].c_str()));

    REQUIRE(gll.status == 'A');
    REQUIRE(rmc.valid);
    REQUIRE(rmc.speed.value == 0);
    REQUIRE(rmc.course.value == 0);
    REQUIRE(gga.fix_quality == 1);
    REQUIRE(gga.satellites_tracked == 8);
    REQUIRE(gsa.fix_type == 3);
    REQUIRE(gsvPage1.total_sats == 8);
    REQUIRE(gsvPage2.total_sats == 8);
}

TEST_CASE("StaticGPS NMEA contract supports southern and western coordinates")
{
    GATAS::NMEAString sentence = "$GPGLL,3330.00000,S,07045.00000,W,183205.50,A";
    addChecksum(sentence);

    minmea_sentence_gll gll = {};
    REQUIRE(validSentence(sentence));
    REQUIRE(minmea_parse_gll(&gll, sentence.c_str()));
    REQUIRE(gll.latitude.value < 0);
    REQUIRE(gll.longitude.value < 0);
}
