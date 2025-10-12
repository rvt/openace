
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#define private public

#include "pico/stdlib.h"

#include "../ace/gdl90service.hpp"
#include "ace/messagerouter.hpp"
#include "ace/messages.hpp"
#include "ace/coreutils.hpp"
#include "geomock.hpp"

class Test : public etl::message_router<Test, GATAS::GdlMsg>
{

public:
    GATAS::GDLData msg;
    etl::imessage_bus *bus;
    bool received = false;
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
        printf("GdlMsg Received\n");
        received = true;
        this->msg = msg.msg;
    }
    void on_receive_unknown(const etl::imessage& msg)
    {
        (void)msg;
    }
};

GATAS::ThreadSafeBus<50> bus;


TEST_CASE( "GDL90.self_test", "[single-file]" )
{
    // GDL90 gdl90;
    // if (!gdl90.self_test())
    // {
    //     REQUIRE( (false) );
    // }
    // REQUIRE( (true) );
}

TEST_CASE( "ownship position", "[single-file]" )
{
    Gdl90Service gdl90Service{&bus};
    gdl90Service.GATAS::PostConstruct();
    Test test{&bus};

    GATAS::OwnshipPositionInfo thisIsUs{CoreUtils::timeUs32(), 0x123456, GATAS::AddressType::FLARM, GATAS::AircraftCategory::Light, false, false, true,
                                          /*lat*/52.99f, /*lon*/4.836f, /*alt*/740, /*vspeed*/540, /*gSpeed*/115 * KN_TO_MS, /*course*/12, /*hTurn*/0, /* Velocity N*/0, /* Velocity E*/0};
    GATAS::OwnshipPositionMsg msg{thisIsUs};

    gdl90Service.on_receive(msg);

// print_buffer_hex<uint8_t>(test.msg);

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

    if ( !gdl90.ownership_or_traffic_report_decode( unpacked, is_ownership, alert_status, addr_type, participant_address,
            latitude, longitude, altitude, misc, nic, nacp, horiz_velocity, vert_velocity, track_hdg,
            emitter, call_sign, emergency_prio_code ) ) REQUIRE( (false) );
    if ( !gdl90.latlon_decode( latitude, latitude_f ) ) REQUIRE( (false) );
    if ( !gdl90.latlon_decode( longitude, longitude_f ) ) REQUIRE( (false) );
    if ( !gdl90.altitude_decode( altitude, altitude_f ) ) REQUIRE( (false) );
    if ( !gdl90.horizontal_velocity_decode( horiz_velocity, horiz_velocity_f ) ) REQUIRE( (false) );
    if ( !gdl90.vertical_velocity_decode( vert_velocity, vert_velocity_f ) ) REQUIRE( (false) );
    if ( !gdl90.track_hdg_decode( track_hdg, track_hdg_f ) ) REQUIRE( (false) );

    REQUIRE( (test.received == true ));
    REQUIRE( (test.msg.size() == 32 ));
    REQUIRE( latitude_f == Catch::Approx(52.99f).margin(0.001) );
    REQUIRE( longitude_f == Catch::Approx(4.836f).margin(0.001) );
    REQUIRE( altitude_f == Catch::Approx(740.f).margin(15) );
    REQUIRE( track_hdg_f == Catch::Approx(12.f).margin(1) );
    REQUIRE( vert_velocity == Catch::Approx(540.f).margin(30) );
    REQUIRE( horiz_velocity_f == Catch::Approx(115.f).margin(1) );
}

TEST_CASE( "heartbeat", "[single-file]" )
{
    CircularPosition center = CircularPosition(20'000, 15, {52.f, 0.f, 0});
    auto mainPos = center.take();

    Gdl90Service gdl90Service{&bus};
    gdl90Service.GATAS::PostConstruct();
    Test test{&bus};

    Gdl90Service::sendHeartBeat(gdl90Service);

//print_buffer_hex<uint8_t>(test.msg);
    REQUIRE( (test.received == true ));
    REQUIRE( (test.msg.size() == 11 ));
}