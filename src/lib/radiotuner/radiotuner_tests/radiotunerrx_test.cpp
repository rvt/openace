
#include <catch2/catch_test_macros.hpp>

#define private public

#include "../ace/radiotunerrx.hpp"
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
    ctx.prioritizeRxTimings();
    REQUIRE(etl::vector<GATAS::DataSource, 1> {} == ctx.dataSourceTimeSlots);

    SECTION("Prioritise should return 500ms on default zone", "[single-file]")
    {
        REQUIRE( ctx.advanceReceiveSlot(CountryRegulations::ZONE0) == 500);
    }
    SECTION("extra FLARM Data Source", "[single-file]")
    {
        ctx.updateDataSources(etl::vector{GATAS::DataSource::FLARM});
        ctx.prioritizeRxTimings();
        REQUIRE(etl::vector{GATAS::DataSource::FLARM} == ctx.dataSourceTimeSlots);
    }

    SECTION("3 Data Sources", "[single-file]")
    {
        ctx.updateDataSources(etl::vector{GATAS::DataSource::FLARM, GATAS::DataSource::OGN1, GATAS::DataSource::ADSL});
        ctx.prioritizeRxTimings();
        REQUIRE(etl::vector{GATAS::DataSource::FLARM, GATAS::DataSource::OGN1, GATAS::DataSource::ADSL} == ctx.dataSourceTimeSlots);

        SECTION("FLARM Data Received", "[single-file]")
        {
            slotReceived[(uint8_t)(GATAS::DataSource::FLARM)] += 1;
            ctx.updateSlotReceive(slotReceived);
            ctx.prioritizeRxTimings();
            REQUIRE(etl::vector{GATAS::DataSource::FLARM, GATAS::DataSource::FLARM, GATAS::DataSource::OGN1, GATAS::DataSource::ADSL} == ctx.dataSourceTimeSlots);

            SECTION("OGN and FLARM Data Received", "[single-file]")
            {
                slotReceived[(uint8_t)(GATAS::DataSource::FLARM)] += 1;
                slotReceived[(uint8_t)(GATAS::DataSource::OGN1)] += 1;
                ctx.updateSlotReceive(slotReceived);
                ctx.prioritizeRxTimings();
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
                ctx.prioritizeRxTimings();
                REQUIRE(etl::vector{GATAS::DataSource::FLARM, GATAS::DataSource::OGN1, GATAS::DataSource::OGN1, GATAS::DataSource::ADSL} == ctx.dataSourceTimeSlots);

                SECTION("NO Data Received", "[single-file]")
                {
                    ctx.updateSlotReceive(slotReceived);
                    ctx.prioritizeRxTimings();
                    REQUIRE(etl::vector{GATAS::DataSource::FLARM, GATAS::DataSource::OGN1, GATAS::DataSource::ADSL} == ctx.dataSourceTimeSlots);
                }

                SECTION("Data sources removed", "[single-file]")
                {
                    ctx.updateDataSources(etl::vector<GATAS::DataSource, 1> {});
                    ctx.prioritizeRxTimings();
                    REQUIRE(etl::vector{etl::vector<GATAS::DataSource, 1>{}} == ctx.dataSourceTimeSlots);
                }
            }
        }
    }
}

