
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/generators/catch_generators.hpp>

#define private public

#include "pico/stdlib.h"

#include "../ace/gdl90service.hpp"
#include "ace/messages.hpp"
#include "ace/coreutils.hpp"
#include "ace/messagerouter.hpp"

#include "geomock.hpp"
#include "mockconfig.h"

class Test : public etl::message_router<Test, GATAS::GdlMsg>
{

public:
    GATAS::GDLData msg;
    etl::imessage_bus *bus;
    uint32_t numReceived = 0;
    Test(etl::imessage_bus *bus_) : bus(bus_)
    {
        bus->subscribe(*this);
    }
    ~Test()
    {
        bus->unsubscribe(*this);
    }

    void on_receive(const GATAS::GdlMsg &msg)
    {
        printf("GdlMsg Received %u", numReceived);
        numReceived += 1;
        this->msg = msg.msg;
    }
    void on_receive_unknown(const etl::imessage &msg)
    {
        (void)msg;
    }
};

GATAS::ThreadSafeBus<50> bus;

// TEST_CASE("GDL90.self_test", "[single-file]")
// {
//     GDL90 gdl90;
//     REQUIRE(gdl90.self_test());
// }

TEST_CASE("ownship position", "[single-file]")
{

    struct Case
    {
        uint32_t icao;
        float lat;
        float lon;
        int16_t alt;
        float speed;
        float vspeed;
        float track;
        const char *callSign;
        const char *name;
    };

    auto tc = GENERATE(

        Case{
            .icao = 0xFF0001,
            .lat = 53.26f,
            .lon = 4.26f,
            .alt = 6000,
            .speed = 50.0f,
            .vspeed = 2.0f,
            .track = 45.f,
            .callSign = "PH-ABC",
            .name = "53.26 4.26"},

        Case{
            .icao = 0xFF0002,
            .lat = 33.97f,
            .lon = -118.034f,
            .alt = 2000, //
            .speed = 127.0f,
            .vspeed = -4.3f,
            .track = 276.5f,
            .callSign = "",
            .name = "33.97 -118.034"},

        Case{
            .icao = 0xFF0003,
            .lat = -31.962f,
            .lon = 115.884f,
            .alt = 12000,
            .speed = 227.0f,
            .vspeed = 9.2f,
            .track = 359.5f,
            .callSign = "1234-567",
            .name = "-31.962 115.884"},

        Case{
            .icao = 0xFF0004,
            .lat = -54.8086f,
            .lon = -68.3344f,
            .alt = 000,
            .speed = 25.0f,
            .vspeed = -12.9f,
            .track = .5f,
            .callSign = "ABCDEFGH",
            .name = "-54.808 -68.334"}

    );

    MockConfig mockConfig{bus};
    mockConfig.ownIcao = tc.icao;
    Gdl90Service gdl90Service{bus, mockConfig};
    gdl90Service.postConstruct();
    Test test{&bus};

    INFO(tc.name);

    GATAS::OwnshipPositionInfo thisIsUs{
        .timestamp = 1700000000, // Unix timestamp
        .lat = tc.lat,           // Amsterdam area
        .lon = tc.lon,
        .ellipseHeight = tc.alt,    // ~1000m MSL + ~47m geoid separation for NL
        .verticalSpeed = tc.vspeed, // Stable cruise
        .groundSpeed = tc.speed,    // ~107 knots, typical light aircraft cruise
        .track = tc.track,          // Heading west
        .hTurnRate = 0.0f,          // Straight and level
        .velocityNorth = 0.0f,      // Pure westward track
        .velocityEast = -55.0f,     // Matches groundSpeed westward
        .geoidSeparation = 47,      // ~47m for the Netherlands (NL geoid is above WGS84)
        .airborne = true,
        .conspicuity = GATAS::Config::Conspicuity{},
    };

    GATAS::OwnshipPositionMsg msg{thisIsUs};

    gdl90Service.on_receive(msg);

    // Receive the message back
    GDL90 gdl90;
    etl::vector<uint8_t, 64> unpacked;
    gdl90.unpack(test.msg, unpacked);

    bool is_ownership = true;
    GDL90::ALERT_STATUS alert_status = GDL90::ALERT_STATUS::__LAST;
    GDL90::ADDR_TYPE addr_type = GDL90::ADDR_TYPE::__LAST;
    uint32_t participant_address = 0xffaa55;
    float latitude_f = -179.2255;
    float longitude_f = +179.4357;
    float altitude_f = 101349;
    uint32_t misc = GDL90::MISC_ALLOWED_MASK;
    GDL90::NIC nic = GDL90::NIC::__LAST;
    GDL90::NACP nacp = GDL90::NACP::__LAST;
    float horiz_velocity_f = 125.4462;
    float vert_velocity_f = -800.333;
    float track_hdg_f = 358.3674;
    GDL90::EMITTER emitter = GDL90::EMITTER::__LAST;
    etl::string<8> call_sign = "";
    GDL90::EMERGENCY_PRIO emergency_prio_code = GDL90::EMERGENCY_PRIO::__LAST;
    uint32_t latitude;
    uint32_t longitude;
    uint32_t altitude;
    uint32_t horiz_velocity;
    uint32_t vert_velocity;
    uint32_t track_hdg;

    REQUIRE(gdl90.ownership_or_traffic_report_decode(unpacked, is_ownership, alert_status, addr_type, participant_address,
                                                     latitude, longitude, altitude, misc, nic, nacp, horiz_velocity, vert_velocity, track_hdg,
                                                     emitter, call_sign, emergency_prio_code));
    REQUIRE(gdl90.latlon_decode(latitude, latitude_f));
    REQUIRE(gdl90.latlon_decode(longitude, longitude_f));
    REQUIRE(gdl90.altitude_decode(altitude, altitude_f));
    REQUIRE(gdl90.horizontal_velocity_decode(horiz_velocity, horiz_velocity_f));
    REQUIRE(gdl90.vertical_velocity_decode(vert_velocity, vert_velocity_f));
    REQUIRE(gdl90.track_hdg_decode(track_hdg, track_hdg_f));

    REQUIRE(test.numReceived == 1);
    REQUIRE(test.msg.size() == 32);
    REQUIRE(latitude_f == Catch::Approx(tc.lat).margin(0.001));
    REQUIRE(longitude_f == Catch::Approx(tc.lon).margin(0.001));
    REQUIRE((altitude_f * FT_TO_M + 47) == Catch::Approx(tc.alt).margin(15));
    REQUIRE(track_hdg_f == Catch::Approx(tc.track).margin(1));
    REQUIRE((vert_velocity_f * FTPMIN_TO_MS) == Catch::Approx(tc.vspeed).margin(0.3));
    REQUIRE((horiz_velocity_f * KN_TO_MS) == Catch::Approx(tc.speed).margin(1));
    REQUIRE(call_sign.length() == etl::string_view(tc.callSign).length());
    REQUIRE(call_sign == etl::string_view(tc.callSign));
}

TEST_CASE("heartbeat", "[single-file]")
{
    MockConfig mockConfig{bus};

    // CircularPosition center = CircularPosition(20'000, 15, {52.f, 0.f, 0});
    // auto mainPos = center.take();

    Gdl90Service gdl90Service{bus, mockConfig};
    gdl90Service.postConstruct();
    Test test{&bus};

    gdl90Service.sendHeartBeat(gdl90Service);

    // print_buffer_hex<uint8_t>(test.msg);
    REQUIRE(test.numReceived == 2);
    REQUIRE(test.msg.size() == 43);
}