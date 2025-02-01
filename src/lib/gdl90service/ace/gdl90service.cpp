#include <stdio.h>

#include "gdl90service.hpp"

/* OpenACE */
#include "ace/coreutils.hpp"
#include "ace/semaphoreguard.hpp"

OpenAce::PostConstruct Gdl90Service::postConstruct()
{
    return OpenAce::PostConstruct::OK;
}

void Gdl90Service::start()
{
    xTaskCreate(gdl90ServiceTask, "gdl90ServiceTask", configMINIMAL_STACK_SIZE + 256, this, tskIDLE_PRIORITY + 2, &taskHandle);
    getBus().subscribe(*this);
};

void Gdl90Service::stop()
{
    getBus().unsubscribe(*this);
    xTaskNotify(taskHandle, TaskState::SHUTDOWN, eSetBits);
}

void Gdl90Service::getData(etl::string_stream &stream, const etl::string_view path) const
{
    (void)path;
    stream << "{";
    stream << "\"heartbeatTx\":" << statistics.heartbeatTx;
    stream << ",\"ownshipPosTx\":" << statistics.ownshipPosTx;
    stream << ",\"trackingAircraftPosTx\":" << statistics.trackingAircraftPosTx;
    stream << ",\"trackingFailureErr\":" << statistics.trackingFailureErr;
    stream << ",\"ownEncodingFailureErr\":" << statistics.ownEncodingFailureErr;
    stream << ",\"heartBeatEncodingFailureErr\":" << statistics.heartBeatEncodingFailureErr;
    stream << "}\n";
}

void Gdl90Service::gdl90ServiceTask(void *arg)
{
    Gdl90Service *gdl90Service = (Gdl90Service *)arg;
    while (true)
    {
        uint32_t notifyValue = ulTaskNotifyTake(pdTRUE, TASK_DELAY_MS(1'000));
        if (notifyValue & TaskState::SHUTDOWN)
        {
            vTaskDelete(nullptr);
            return;
        }
        else
        {
            sendHeartBeat(*gdl90Service);
        }
    }
}

GDL90::EMITTER aircraftTypeToEmitter(OpenAce::AircraftCategory at)
{
    switch (at)
    {
    case OpenAce::AircraftCategory::Unknown:
        return GDL90::EMITTER::UNKNOWN;
    case OpenAce::AircraftCategory::GliderMotorGlider:
        return GDL90::EMITTER::LIGHT;
    case OpenAce::AircraftCategory::TowPlane:
        return GDL90::EMITTER::LIGHT;
    case OpenAce::AircraftCategory::Helicopter:
        return GDL90::EMITTER::ROTOCRAFT;
    case OpenAce::AircraftCategory::Skydiver:
        return GDL90::EMITTER::PARACHUTIST;
    case OpenAce::AircraftCategory::DropPlane:
        return GDL90::EMITTER::LIGHT;
    case OpenAce::AircraftCategory::HangGlider:
        return GDL90::EMITTER::ULTRA_LIGHT;
    case OpenAce::AircraftCategory::Paraglider:
        return GDL90::EMITTER::ULTRA_LIGHT;
    case OpenAce::AircraftCategory::ReciprocatingEngine:
        return GDL90::EMITTER::LIGHT;
    case OpenAce::AircraftCategory::JetTurbopropEngine:
        return GDL90::EMITTER::LIGHT;
    case OpenAce::AircraftCategory::Balloon:
        return GDL90::EMITTER::LIGHTER_THAN_AIR;
    case OpenAce::AircraftCategory::Airship:
        return GDL90::EMITTER::LIGHTER_THAN_AIR;
    case OpenAce::AircraftCategory::Uav:
        return GDL90::EMITTER::UNMANNED;
    case OpenAce::AircraftCategory::StaticObstacle:
        return GDL90::EMITTER::POINT_OBSTACLE;
    default:
        return GDL90::EMITTER::UNKNOWN;
    }
}

void Gdl90Service::on_receive(const OpenAce::ConfigUpdatedMsg &msg)
{
    if (msg.moduleName != Gdl90Service::NAME)
    {
        return;
    }

    if (auto guard = SemaphoreGuard<portMAX_DELAY>(BaseModule::configMutex))
    {
        auto openAceConfiguration = msg.config.openAceConfig();
        type = openAceConfiguration.addressType == OpenAce::AddressType::ICAO ? GDL90::ADDR_TYPE::ADSB_WITH_ICAO_ADDR : GDL90::ADDR_TYPE::ADSB_WITH_SELF_ADDR;
        address = openAceConfiguration.address;
        category = openAceConfiguration.category;
    }
}

void Gdl90Service::on_receive(const OpenAce::OwnshipPositionMsg &msg)
{
    const OpenAce::OwnshipPositionInfo &pos = msg.position;

    OpenAce::IcaoAddress icaoAddress("");
    uint32_t latitude;
    uint32_t longitude;
    uint32_t altitude;
    uint32_t horiz_velocity;
    uint32_t vert_velocity;
    uint32_t track_hdg;

    gdl90.latlon_encode(latitude, pos.lat);
    gdl90.latlon_encode(longitude, pos.lon);
    gdl90.altitude_encode(altitude, pos.altitudeWgs84 * M_TO_FT);
    gdl90.horizontal_velocity_encode(horiz_velocity, pos.groundSpeed * MS_TO_KN);
    gdl90.vertical_velocity_encode(vert_velocity, pos.verticalSpeed * MS_TO_FTPMIN);
    gdl90.track_hdg_encode(track_hdg, pos.course);

    GDL90::RawBytes unpacked;
    if (gdl90.ownership_or_traffic_report_encode(
            unpacked,
            true,
            GDL90::ALERT_STATUS::INACTIVE,
            type,
            address,
            latitude,
            longitude,
            altitude,
            GDL90::MISC_TT_HEADING_TRUE_MASK | (pos.groundSpeed > OpenAce::GROUNDSPEED_CONSIDERING_AIRBORN ? GDL90::MISC_AIRBORNE_MASK : 0),
            GDL90::NIC::HPL_LT_25_VPL_LT_37_5,
            GDL90::NACP::HFOM_LT_30_VFOM_LT_45, /* Integrity | Accuracy We do not really have this information from the GPS */
            horiz_velocity,
            vert_velocity,
            track_hdg,
            aircraftTypeToEmitter(category),
            icaoAddress, // [0-9A-Z]
            GDL90::EMERGENCY_PRIO::NO_EMERGENCY))
    {
        packAndSend(unpacked);
        statistics.ownshipPosTx++;
    }
    else
    {
        statistics.ownEncodingFailureErr++;
    }

    // Send Geo Altitude
    uint32_t geo_altitude;
    bool vertical_warning = true;
    float vertical_figure_of_merit_f = 0;
    uint32_t vertical_figure_of_merit;
    gdl90.geo_altitude_encode(geo_altitude, pos.altitudeWgs84 * M_TO_FT);
    gdl90.vertical_figure_of_merit_encode(vertical_figure_of_merit, vertical_figure_of_merit_f);
    if (gdl90.ownership_geometric_altitude_encode(unpacked, geo_altitude, vertical_warning, vertical_figure_of_merit))
    {
        packAndSend(unpacked);
    }
    else
    {
        statistics.ownEncodingFailureErr++;
    }
}

void Gdl90Service::on_receive(const OpenAce::TrackedAircraftPositionMsg &msg)
{
    const OpenAce::AircraftPositionInfo &pos = msg.position;

    uint32_t latitude;
    uint32_t longitude;
    uint32_t altitude;
    uint32_t horiz_velocity;
    uint32_t vert_velocity;
    uint32_t track_hdg;

    // printf("lat:%f lon:%f alt:%d addr:%d\n", pos.lat, pos.lon, pos.altitudeWgs84, pos.address);

    gdl90.latlon_encode(latitude, pos.lat);
    gdl90.latlon_encode(longitude, pos.lon);
    gdl90.altitude_encode(altitude, pos.altitudeWgs84 * M_TO_FT);
    gdl90.horizontal_velocity_encode(horiz_velocity, pos.groundSpeed * MS_TO_KN);
    gdl90.vertical_velocity_encode(vert_velocity, pos.verticalSpeed * MS_TO_FTPMIN);
    gdl90.track_hdg_encode(track_hdg, pos.course);
    GDL90::ADDR_TYPE type = pos.addressType == OpenAce::AddressType::ICAO ? GDL90::ADDR_TYPE::ADSB_WITH_ICAO_ADDR : GDL90::ADDR_TYPE::ADSB_WITH_SELF_ADDR;

    GDL90::RawBytes unpacked;
    if (gdl90.ownership_or_traffic_report_encode(unpacked,
                                                 false,
                                                 GDL90::ALERT_STATUS::ACTIVE,
                                                 type,
                                                 pos.address,
                                                 latitude,
                                                 longitude,
                                                 altitude,
                                                 GDL90::MISC_TT_HEADING_TRUE_MASK | GDL90::MISC_REPORT_UPDATED_MASK | (pos.groundSpeed > OpenAce::GROUNDSPEED_CONSIDERING_AIRBORN ? GDL90::MISC_AIRBORNE_MASK : 0),
                                                 GDL90::NIC::HPL_LT_25_VPL_LT_37_5,
                                                 GDL90::NACP::HFOM_LT_30_VFOM_LT_45, /* Integrity | Accuracy We do not really have this information from the GPS */
                                                 horiz_velocity,
                                                 vert_velocity,
                                                 track_hdg,
                                                 aircraftTypeToEmitter(pos.aircraftType),
                                                 pos.icaoAddress, /* // [0-9A-Z] TODO: Can we use DDB ?? */
                                                 GDL90::EMERGENCY_PRIO::NO_EMERGENCY))
    {
        packAndSend(unpacked);
        statistics.trackingAircraftPosTx++;
    }
    else
    {
        statistics.trackingFailureErr++;
    }
}

// ownership_geometric_altitude_encode(RawBytes& unpacked, uint32_t  geo_altitude, bool  vertical_warning, uint32_t  vertical_figure_of_merit );
// height_above_terrain_encode(RawBytes& unpacked, uint32_t  height );

void Gdl90Service::sendHeartBeat(Gdl90Service &gdl90Service)
{
    // https://www.faa.gov/sites/faa.gov/files/air_traffic/technology/adsb/archival/GDL90_Public_ICD_RevA.PDF
    uint32_t status = GDL90::HEARTBEAT_STATUS_UAT_INITIALIZED_MASK |
                      /* GDL90::HEARTBEAT_STATUS_RATCS_MASK | */
                      /* GDL90::HEARTBEAT_STATUS_GPS_BAT_LOW_MASK | */
                      /* GDL90::HEARTBEAT_STATUS_ADDR_TYPE_MASK | */
                      /* GDL90::HEARTBEAT_STATUS_IDENT_MASK | */
                      /* GDL90::HEARTBEAT_STATUS_MAINT_REQD_MASK | */ /* Can be used to indicate device is not good*/
                      GDL90::HEARTBEAT_STATUS_GPS_POS_VALID_MASK |
                      /* GDL90::HEARTBEAT_STATUS_UAT_OK_MASK | */
                      /* GDL90::HEARTBEAT_STATUS_CSA_NOT_AVAIL_MASK | */
                      /*GDL90::HEARTBEAT_STATUS_CSA_REQUESTED_MASK*/
                      0;

    GDL90::RawBytes unpacked;

    // Send heartBeat
    if (gdl90Service.gdl90.heartbeat_encode(unpacked, status, CoreUtils::secondsSinceEpoch() % (24 * 3600), 0, 0))
    {
        gdl90Service.packAndSend(unpacked);
        gdl90Service.statistics.heartbeatTx++;
    }
    else
    {
        gdl90Service.statistics.heartBeatEncodingFailureErr++;
    }

    // Send ForeFLight heartbeat
    if (gdl90Service.gdl90.foreflight_id_encode(unpacked, 12345, "OpenAce", "OpenAce Device", 1))
    {
        gdl90Service.packAndSend(unpacked);
        gdl90Service.statistics.heartbeatTx++;
    }
    else
    {
        gdl90Service.statistics.heartBeatEncodingFailureErr++;
    }
}

void Gdl90Service::packAndSend(const GDL90::RawBytes &unpacked)
{
    OpenAce::GdlMsg GdlMsg{};
    gdl90.pack(GdlMsg.msg, unpacked);
    getBus().receive(GdlMsg);
}
