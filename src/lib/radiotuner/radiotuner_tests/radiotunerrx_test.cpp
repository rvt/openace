
#include <catch2/catch_test_macros.hpp>

#define private public

#include "../ace/radiotunerrx.hpp"
#include "ace/messagerouter.hpp"
#include "ace/messages.hpp"
#include "pico/time.h"
#include "etl/vector.h"

using RadioProtocolCtx = RadioTunerRx::RadioProtocolCtx;
GATAS::ThreadSafeBus<50> bus;
GATAS::OwnshipPositionMsg ownshipPosition{};

TEST_CASE("RadioProtocolCtx", "[single-file]")
{
    RadioProtocolCtx ctx{nullptr, nullptr};

    etl::array<uint8_t, (uint8_t)GATAS::DataSource::_TRANSPROTOCOLS> slotReceived = {};

    ctx.updateDataSources(etl::vector<GATAS::DataSource, 1> {});
    ctx.prioritizeDatasources();
    REQUIRE(etl::vector<GATAS::DataSource, 1> {} == ctx.dataSourceTimeSlots);

    SECTION("Prioritise should return 500ms on default zone", "[single-file]")
    {
        REQUIRE( ctx.advanceReceiveSlot(CountryRegulations::ZONE0) == 500);
    }
    SECTION("extra FLARM Data Source", "[single-file]")
    {
        ctx.updateDataSources(etl::vector{GATAS::DataSource::FLARM});
        ctx.prioritizeDatasources();
        REQUIRE(etl::vector{GATAS::DataSource::FLARM} == ctx.dataSourceTimeSlots);
    }

    SECTION("3 Data Sources", "[single-file]")
    {
        ctx.updateDataSources(etl::vector{GATAS::DataSource::FLARM, GATAS::DataSource::OGN1, GATAS::DataSource::ADSL});
        ctx.prioritizeDatasources();
        REQUIRE(etl::vector{GATAS::DataSource::FLARM, GATAS::DataSource::OGN1, GATAS::DataSource::ADSL} == ctx.dataSourceTimeSlots);

        SECTION("FLARM Data Received", "[single-file]")
        {
            slotReceived[(uint8_t)(GATAS::DataSource::FLARM)] += 1;
            ctx.updateSlotReceive(slotReceived);
            ctx.prioritizeDatasources();
            REQUIRE(etl::vector{GATAS::DataSource::FLARM, GATAS::DataSource::FLARM, GATAS::DataSource::OGN1, GATAS::DataSource::ADSL} == ctx.dataSourceTimeSlots);

            SECTION("OGN and FLARM Data Received", "[single-file]")
            {
                slotReceived[(uint8_t)(GATAS::DataSource::FLARM)] += 1;
                slotReceived[(uint8_t)(GATAS::DataSource::OGN1)] += 1;
                ctx.updateSlotReceive(slotReceived);
                ctx.prioritizeDatasources();
                REQUIRE(etl::vector{GATAS::DataSource::FLARM, GATAS::DataSource::FLARM, GATAS::DataSource::OGN1, GATAS::DataSource::OGN1, GATAS::DataSource::ADSL} == ctx.dataSourceTimeSlots);
                REQUIRE(*ctx.upcomingDataSource == GATAS::DataSource::FLARM);

                SECTION("Should be circular receive slots", "[single-file]")
                {
                    ctx.advanceReceiveSlot(CountryRegulations::Zone::ZONE1);
                    REQUIRE(*ctx.upcomingDataSource == GATAS::DataSource::FLARM);
                    ctx.advanceReceiveSlot(CountryRegulations::Zone::ZONE1);
                    REQUIRE(*ctx.upcomingDataSource == GATAS::DataSource::OGN1);
                    ctx.advanceReceiveSlot(CountryRegulations::Zone::ZONE1);
                    REQUIRE(*ctx.upcomingDataSource == GATAS::DataSource::OGN1);
                    ctx.advanceReceiveSlot(CountryRegulations::Zone::ZONE1);
                    REQUIRE(*ctx.upcomingDataSource == GATAS::DataSource::ADSL);
                    ctx.advanceReceiveSlot(CountryRegulations::Zone::ZONE1);
                    // Here prioritsation should happen
                    REQUIRE(*ctx.upcomingDataSource == GATAS::DataSource::FLARM);
                    ctx.advanceReceiveSlot(CountryRegulations::Zone::ZONE1);
                    REQUIRE(*ctx.upcomingDataSource == GATAS::DataSource::FLARM);
                    ctx.advanceReceiveSlot(CountryRegulations::Zone::ZONE1);
                    REQUIRE(*ctx.upcomingDataSource == GATAS::DataSource::OGN1);
                }
            }

            SECTION("OGN Data, no FLARM Received", "[single-file]")
            {
                slotReceived[(uint8_t)(GATAS::DataSource::OGN1)] += 1;
                ctx.updateSlotReceive(slotReceived);
                ctx.prioritizeDatasources();
                REQUIRE(etl::vector{GATAS::DataSource::FLARM, GATAS::DataSource::OGN1, GATAS::DataSource::OGN1, GATAS::DataSource::ADSL} == ctx.dataSourceTimeSlots);

                SECTION("NO Data Received", "[single-file]")
                {
                    ctx.updateSlotReceive(slotReceived);
                    ctx.prioritizeDatasources();
                    REQUIRE(etl::vector{GATAS::DataSource::FLARM, GATAS::DataSource::OGN1, GATAS::DataSource::ADSL} == ctx.dataSourceTimeSlots);
                }

                SECTION("Data sources removed", "[single-file]")
                {
                    ctx.updateDataSources(etl::vector<GATAS::DataSource, 1> {});
                    ctx.prioritizeDatasources();
                    REQUIRE(etl::vector{etl::vector<GATAS::DataSource, 1>{}} == ctx.dataSourceTimeSlots);
                }
            }
        }
    }
}
// TEST_CASE( "When ownship is received", "[single-file]" )
// {
//     auto radioController = RadioTunerRx{&bus};
//     radioController.numRadios = 2;
//     radioController.addRadioStructures();

//     // Should Update the current regulation
//     get_absolute_time_value = 100000;
//     ownshipPosition.position.lat = 51;
//     ownshipPosition.position.lon = 4;
//     radioController.on_receive(ownshipPosition);
//     REQUIRE( (radioController.currentZone == CountryRegulations::ZONE1) );

//     SECTION( "Should not update" )
//     {
//         get_absolute_time_value = 200000;

//         ownshipPosition.position.lat = 51;
//         ownshipPosition.position.lon = 170;
//         REQUIRE( (radioController.currentZone == CountryRegulations::ZONE1) );

//         SECTION( "should update to ZONE3 after time passed" )
//         {
//             get_absolute_time_value =300000;
//             for (int i=0; i<100; i++)
//             {
//                 radioController.on_receive(ownshipPosition);
//             }
//             REQUIRE( (radioController.currentZone == CountryRegulations::Zone::ZONE3) );
//         }
//     }
// }

// TEST_CASE( "Should find least occupied radio", "[single-file]" )
// {
//     auto radioController = RadioTunerRx{&bus};
//     radioController.numRadios = 2;
//     radioController.addRadioStructures();

//     REQUIRE( (radioController.leastOccupiedRadio() == 0) );
//     radioController.startListen(GATAS::DataSource::OGN1, 0);
//     REQUIRE( (radioController.leastOccupiedRadio() == 1) );
//     radioController.startListen(GATAS::DataSource::FLARM, 1);
//     REQUIRE( (radioController.leastOccupiedRadio() == 0) );
//     radioController.startListen(GATAS::DataSource::PAW, 0);
//     REQUIRE( (radioController.leastOccupiedRadio() == 1) );

//     SECTION( "When one radio" )
//     {
//         auto radioController = RadioTunerRx{&bus};
//         radioController.startListen(GATAS::DataSource::OGN1);
//         REQUIRE( (radioController.leastOccupiedRadio() == 0) );
//     }
// }

// TEST_CASE( "Listen on datasource", "[single-file]" )
// {
//     auto radioController = RadioTunerRx{&bus};
//     radioController.numRadios = 2;
//     radioController.addRadioStructures();

//     REQUIRE( (radioController.numberOfSlots(GATAS::DataSource::OGN1) == 0) );
//     REQUIRE( (radioController.numberOfSlots(GATAS::DataSource::FLARM) == 0) );
//     radioController.startListen(GATAS::DataSource::OGN1);
//     REQUIRE( (radioController.numberOfSlots(GATAS::DataSource::FLARM) == 0) );
//     radioController.startListen(GATAS::DataSource::FLARM);
//     REQUIRE( (radioController.numberOfSlots(GATAS::DataSource::OGN1) == 1) );
//     REQUIRE( (radioController.numberOfSlots(GATAS::DataSource::FLARM) == 1) );

//     SECTION( "When stop listening" )
//     {
//         auto radioController = RadioTunerRx{&bus};
//         radioController.numRadios = 2;
//         radioController.addRadioStructures();

//         radioController.startListen(GATAS::DataSource::OGN1);
//         radioController.startListen(GATAS::DataSource::FLARM);
//         REQUIRE( (radioController.numberOfSlots(GATAS::DataSource::OGN1) == 1) );
//         REQUIRE( (radioController.numberOfSlots(GATAS::DataSource::FLARM) == 1) );
//         radioController.stopListen(GATAS::DataSource::OGN1);
//         REQUIRE( (radioController.numberOfSlots(GATAS::DataSource::OGN1) == 0) );
//         REQUIRE( (radioController.numberOfSlots(GATAS::DataSource::FLARM) == 1) );
//     }

//     SECTION( "When startListen is done twice" )
//     {
//         auto radioController = RadioTunerRx{&bus};
//         radioController.numRadios = 2;
//         radioController.addRadioStructures();

//         radioController.startListen(GATAS::DataSource::OGN1);
//         radioController.startListen(GATAS::DataSource::OGN1);
//         REQUIRE( (radioController.numberOfSlots(GATAS::DataSource::OGN1) == 1) );

//         radioController.startListen(GATAS::DataSource::OGN1, -2);
//         REQUIRE( (radioController.numberOfSlots(GATAS::DataSource::OGN1) == 1) );
//     }
// }

// TEST_CASE ("Timer slots", "[single-file]")
// {
//     auto radioController = RadioTunerRx{&bus};
//     radioController.numRadios = 2;
//     radioController.addRadioStructures();
//     radioController.startListen(GATAS::DataSource::OGN1);
// //    printf("%s-\n", radioController.visualizeTimeSlots(radioController.radioTasks.at(0)).c_str());
//     REQUIRE( (strcmp(radioController.visualizeTimeSlots(radioController.radioTasks.at(0)).c_str(), "l1c0:O") == 0 ) );

//     radioController.startListen(GATAS::DataSource::FLARM);
//     radioController.startListen(GATAS::DataSource::OGN1);
//     // printf("%s-\n", radioController.visualizeTimeSlots(radioController.radioTasks.at(0)).c_str());
//     // printf("%s-\n", radioController.visualizeTimeSlots(radioController.radioTasks.at(1)).c_str());
//     REQUIRE( (strcmp(radioController.visualizeTimeSlots(radioController.radioTasks.at(0)).c_str(), "l1c0:O") == 0 ) );
//     REQUIRE( (strcmp(radioController.visualizeTimeSlots(radioController.radioTasks.at(1)).c_str(), "l1c0:F") == 0 ) );

//     radioController.startListen(GATAS::DataSource::ADSL);
//     // printf("%s-\n", radioController.visualizeTimeSlots(radioController.radioTasks.at(0)).c_str());
//     // printf("%s-\n", radioController.visualizeTimeSlots(radioController.radioTasks.at(1)).c_str());
//     REQUIRE( (strcmp(radioController.visualizeTimeSlots(radioController.radioTasks.at(0)).c_str(), "l2c0:OA") == 0 ) );
//     REQUIRE( (strcmp(radioController.visualizeTimeSlots(radioController.radioTasks.at(1)).c_str(), "l1c0:F") == 0 ) );

//     SECTION( "When OGN Datasource Received", "[single-file]")
//     {
//         GATAS::AircraftPositionInfo position= {};
//         position.dataSource = GATAS::DataSource::OGN1;
//         radioController.on_receive(GATAS::AircraftPositionMsg{position});
//         radioController.prioritizeDatasources();
//         // printf("%s-\n", radioController.visualizeTimeSlots(radioController.radioTasks.at(0)).c_str());
//         // printf("%s-\n", radioController.visualizeTimeSlots(radioController.radioTasks.at(1)).c_str());
//         REQUIRE( (strcmp(radioController.visualizeTimeSlots(radioController.radioTasks.at(0)).c_str(), "l3c0:OOA") == 0 ) );
//         REQUIRE( (strcmp(radioController.visualizeTimeSlots(radioController.radioTasks.at(1)).c_str(), "l1c0:F") == 0 ) );

//         SECTION( "When only FLARM is received Received and not OGN", "[single-file]")
//         {
//             position.dataSource = GATAS::DataSource::FLARM;
//             radioController.on_receive(GATAS::AircraftPositionMsg{position});
//             radioController.prioritizeDatasources();
//             // printf("%s-\n", radioController.visualizeTimeSlots(radioController.radioTasks.at(0)).c_str());
//             // printf("%s-\n", radioController.visualizeTimeSlots(radioController.radioTasks.at(1)).c_str());
//             REQUIRE( (strcmp(radioController.visualizeTimeSlots(radioController.radioTasks.at(0)).c_str(), "l2c0:OA") == 0 ) );
//             REQUIRE( (strcmp(radioController.visualizeTimeSlots(radioController.radioTasks.at(1)).c_str(), "l2c0:FF") == 0 ) );

//             // Nothing received anymore
//             radioController.prioritizeDatasources();
//             // printf("%s-\n", radioController.visualizeTimeSlots(radioController.radioTasks.at(0)).c_str());
//             // printf("%s-\n", radioController.visualizeTimeSlots(radioController.radioTasks.at(1)).c_str());
//             REQUIRE( (strcmp(radioController.visualizeTimeSlots(radioController.radioTasks.at(0)).c_str(), "l2c0:OA") == 0 ) );
//             REQUIRE( (strcmp(radioController.visualizeTimeSlots(radioController.radioTasks.at(1)).c_str(), "l1c0:F") == 0 ) );
//         }

//         SECTION( "When in middle of cycle, should not prioritize datasources", "[single-file]")
//         {
//             REQUIRE( (strcmp(radioController.visualizeTimeSlots(radioController.radioTasks.at(0)).c_str(), "l3c0:OOA") == 0 ) );
//             position.dataSource = GATAS::DataSource::OGN1;
//             radioController.on_receive(GATAS::AircraftPositionMsg{position});
//             radioController.radioTasks.at(0).slotIdx = 1;
//             radioController.prioritizeDatasources();
//             printf("%s-\n", radioController.visualizeTimeSlots(radioController.radioTasks.at(0)).c_str());
//             REQUIRE( (strcmp(radioController.visualizeTimeSlots(radioController.radioTasks.at(0)).c_str(), "l3c1:OOA") == 0 ) );
//         }
//     }

// }
