
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

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
MockConfig mockConfig{bus};

// TEST_CASE("GDL90.self_test", "[single-file]")
// {
//     GDL90 gdl90;
//     REQUIRE(gdl90.self_test());
// }

TEST_CASE("ownship position", "[single-file]")
{
    Gdl90Service gdl90Service{bus, mockConfig};
    gdl90Service.postConstruct();
    Test test{&bus};

    GATAS::OwnshipPositionInfo thisIsUs{
        .timestamp = 1700000000, // Unix timestamp
        .lat = 52.3676f,         // Amsterdam area
        .lon = 4.9041f,
        .ellipseHeight = 1047,  // ~1000m MSL + ~47m geoid separation for NL
        .verticalSpeed = 12.6f,  // Stable cruise
        .groundSpeed = 55.2f,   // ~107 knots, typical light aircraft cruise
        .track = 270.0f,        // Heading west
        .hTurnRate = 0.0f,      // Straight and level
        .velocityNorth = 0.0f,  // Pure westward track
        .velocityEast = -55.0f, // Matches groundSpeed westward
        .geoidSeparation = 47,  // ~47m for the Netherlands (NL geoid is above WGS84)
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
    etl::string<8> call_sign = "N53587";
    GDL90::EMERGENCY_PRIO emergency_prio_code = GDL90::EMERGENCY_PRIO::__LAST;
    uint32_t latitude;
    uint32_t longitude;
    uint32_t altitude;
    uint32_t horiz_velocity;
    uint32_t vert_velocity;
    uint32_t track_hdg;

    if (!gdl90.ownership_or_traffic_report_decode(unpacked, is_ownership, alert_status, addr_type, participant_address,
                                                  latitude, longitude, altitude, misc, nic, nacp, horiz_velocity, vert_velocity, track_hdg,
                                                  emitter, call_sign, emergency_prio_code))
        REQUIRE((false));
    if (!gdl90.latlon_decode(latitude, latitude_f))
        REQUIRE((false));
    if (!gdl90.latlon_decode(longitude, longitude_f))
        REQUIRE((false));
    if (!gdl90.altitude_decode(altitude, altitude_f))
        REQUIRE((false));
    if (!gdl90.horizontal_velocity_decode(horiz_velocity, horiz_velocity_f))
        REQUIRE((false));
    if (!gdl90.vertical_velocity_decode(vert_velocity, vert_velocity_f))
        REQUIRE((false));
    if (!gdl90.track_hdg_decode(track_hdg, track_hdg_f))
        REQUIRE((false));

    REQUIRE(test.numReceived == 1);
    REQUIRE(test.msg.size() == 32);
    REQUIRE(latitude_f == Catch::Approx(52.3676f).margin(0.001));
    REQUIRE(longitude_f == Catch::Approx(4.9041f).margin(0.001));
    REQUIRE((altitude_f * FT_TO_M + 47) == Catch::Approx(1047.f).margin(15));
    REQUIRE(track_hdg_f == Catch::Approx(270.f).margin(1));
    REQUIRE((vert_velocity_f * FTPMIN_TO_MS) == Catch::Approx(12.6f).margin(0.3));
    REQUIRE((horiz_velocity_f * KN_TO_MS) == Catch::Approx(55.2f).margin(1));
}

TEST_CASE("heartbeat", "[single-file]")
{
    // CircularPosition center = CircularPosition(20'000, 15, {52.f, 0.f, 0});
    // auto mainPos = center.take();

    Gdl90Service gdl90Service{bus, mockConfig};
    gdl90Service.postConstruct();
    Test test{&bus};

    gdl90Service.sendHeartBeat(gdl90Service);

    // print_buffer_hex<uint8_t>(test.msg);
    REQUIRE( test.numReceived == 2 );
    REQUIRE( test.msg.size() == 43 );
}