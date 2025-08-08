#include <stdio.h>

#include "../gdl90service.hpp"

/* GATAS */
#include "ace/coreutils.hpp"
#include "ace/semaphoreguard.hpp"

GATAS::PostConstruct Gdl90Service::postConstruct()
{
    return GATAS::PostConstruct::OK;
}

void Gdl90Service::start()
{
    xTaskCreate(gdl90ServiceTask, Gdl90Service::NAME.cbegin(), configMINIMAL_STACK_SIZE + 256, this, tskIDLE_PRIORITY + 2, &taskHandle);
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
            gdl90Service->sendHeartBeat(*gdl90Service);
        }
    }
}

GDL90::EMITTER Gdl90Service::aircraftTypeToEmitter(GATAS::AircraftCategory category) const
{
    switch (category)
    {
    case GATAS::AircraftCategory::UNKNOWN:
        return GDL90::EMITTER::UNKNOWN;
    case GATAS::AircraftCategory::LIGHT:
        return GDL90::EMITTER::LIGHT;
    case GATAS::AircraftCategory::SMALL:
        return GDL90::EMITTER::SMALL;
    case GATAS::AircraftCategory::LARGE:
        return GDL90::EMITTER::LARGE;
    case GATAS::AircraftCategory::HIGH_VORTEX:
        return GDL90::EMITTER::HIGH_VORTEX_LARGE;
    case GATAS::AircraftCategory::HEAVY_ICAO:
        return GDL90::EMITTER::HEAVY;
    case GATAS::AircraftCategory::AEROBATIC:
        return GDL90::EMITTER::HIGHLY_MANEUVERABLE;
    case GATAS::AircraftCategory::ROTORCRAFT:
        return GDL90::EMITTER::ROTOCRAFT;
    case GATAS::AircraftCategory::GLIDER:
        return GDL90::EMITTER::GLIDER_SAILPLANE;
    case GATAS::AircraftCategory::LIGHT_THAN_AIR:
        return GDL90::EMITTER::LIGHTER_THAN_AIR;
    case GATAS::AircraftCategory::SKY_DIVER:
        return GDL90::EMITTER::PARACHUTIST;
    case GATAS::AircraftCategory::ULTRA_LIGHT_FIXED_WING:
        return GDL90::EMITTER::ULTRA_LIGHT;
    case GATAS::AircraftCategory::UN_MANNED:
        return GDL90::EMITTER::UNMANNED;
    case GATAS::AircraftCategory::SPACE_VEHICLE:
        return GDL90::EMITTER::SPACE;
    case GATAS::AircraftCategory::SURFACE_EMERGENCY_VEHICLE:
        return GDL90::EMITTER::SURFACE_EMERGENCY;
    case GATAS::AircraftCategory::SURFACE_VEHICLE:
        return GDL90::EMITTER::SURFACE_SERVICE;
    case GATAS::AircraftCategory::POINT_OBSTACLE:
        return GDL90::EMITTER::POINT_OBSTACLE;
    case GATAS::AircraftCategory::CLUSTER_OBSTACLE:
        return GDL90::EMITTER::CLUSTER_OBSTACLE;
    case GATAS::AircraftCategory::LINE_OBSTACLE:
        return GDL90::EMITTER::LINE_OBSTACLE;
    // Handle custom values
    case GATAS::AircraftCategory::GYROCOPTER:
        return GDL90::EMITTER::ROTOCRAFT;
    case GATAS::AircraftCategory::HANG_GLIDER:
        return GDL90::EMITTER::GLIDER_SAILPLANE;
    case GATAS::AircraftCategory::PARA_GLIDER:
        return GDL90::EMITTER::GLIDER_SAILPLANE;
    case GATAS::AircraftCategory::DROP_PLANE:
        return GDL90::EMITTER::LIGHT;
    case GATAS::AircraftCategory::MILITARY:
        return GDL90::EMITTER::HEAVY; // or could be LARGE
    default:
        // For any unhandled values, return UNKNOWN
        return GDL90::EMITTER::UNKNOWN;
    }
}

void Gdl90Service::on_receive(const GATAS::OwnshipPositionMsg &msg)
{
    const GATAS::OwnshipPositionInfo &pos = msg.position;
    geoidSeparation = pos.geoidSeparation;

    uint32_t latitude;
    uint32_t longitude;
    uint32_t altitude;
    uint32_t horiz_velocity;
    uint32_t vert_velocity;
    uint32_t track_hdg;

    auto type = pos.conspicuity.addressType == GATAS::AddressType::ICAO ? GDL90::ADDR_TYPE::ADSB_WITH_ICAO_ADDR : GDL90::ADDR_TYPE::ADSB_WITH_SELF_ADDR;

    gdl90.latlon_encode(latitude, pos.lat);
    gdl90.latlon_encode(longitude, pos.lon);
    gdl90.altitude_encode(altitude, pos.ellipseHeight * M_TO_FT);
    gdl90.horizontal_velocity_encode(horiz_velocity, pos.groundSpeed * MS_TO_KN);
    gdl90.vertical_velocity_encode(vert_velocity, pos.verticalSpeed * MS_TO_FTPMIN);
    gdl90.track_hdg_encode(track_hdg, pos.course);
    GDL90::RawBytes unpacked;
    if (gdl90.ownership_or_traffic_report_encode(
            unpacked,
            true,
            GDL90::ALERT_STATUS::INACTIVE,
            type,
            pos.conspicuity.address,
            latitude,
            longitude,
            altitude,
            GDL90::MISC_TT_HEADING_TRUE_MASK | (pos.groundSpeed > GATAS::GROUNDSPEED_CONSIDERING_AIRBORN ? GDL90::MISC_AIRBORNE_MASK : 0),
            GDL90::NIC::HPL_LT_25_VPL_LT_37_5,
            GDL90::NACP::HFOM_LT_30_VFOM_LT_45, /* Integrity | Accuracy We do not really have this information from the GPS */
            horiz_velocity,
            vert_velocity,
            track_hdg,
            aircraftTypeToEmitter(pos.conspicuity.category),
            "", // [0-9A-Z ]
            GDL90::EMERGENCY_PRIO::NO_EMERGENCY))
    {
        packAndSend(unpacked);
        statistics.ownshipPosTx += 1;
    }
    else
    {
        statistics.ownEncodingFailureErr += 1;
    }

    if (gpsStatusValid)
    {
        // Send Geo Altitude
        uint32_t geo_altitude;
        bool vertical_warning = true;
        constexpr float vertical_figure_of_merit_f = 0;
        uint32_t vertical_figure_of_merit;
        gdl90.geo_altitude_encode(geo_altitude, pos.ellipseHeight * M_TO_FT);
        gdl90.vertical_figure_of_merit_encode(vertical_figure_of_merit, vertical_figure_of_merit_f);
        if (gdl90.ownership_geometric_altitude_encode(unpacked, geo_altitude, vertical_warning, vertical_figure_of_merit))
        {
            packAndSend(unpacked);
        }
        else
        {
            statistics.ownEncodingFailureErr += 1;
        }
    }
}

void Gdl90Service::on_receive(const GATAS::GpsStatsMsg &msg)
{
    gpsStatusValid = msg.fixType == 3;
}

void Gdl90Service::on_receive(const GATAS::TrackedAircraftPositionMsg &msg)
{
    const GATAS::AircraftPositionInfo &pos = msg.position;

    uint32_t latitude;
    uint32_t longitude;
    uint32_t altitude;
    uint32_t horiz_velocity;
    uint32_t vert_velocity;
    uint32_t track_hdg;

    // printf("lat:%f lon:%f alt:%d addr:%ld\n", pos.lat, pos.lon, pos.altitudeGeoid, pos.address);

    gdl90.latlon_encode(latitude, pos.lat);
    gdl90.latlon_encode(longitude, pos.lon);
    gdl90.altitude_encode(altitude, (pos.ellipseHeight + geoidSeparation) * M_TO_FT);
    gdl90.horizontal_velocity_encode(horiz_velocity, pos.groundSpeed * MS_TO_KN);
    gdl90.vertical_velocity_encode(vert_velocity, pos.verticalSpeed * MS_TO_FTPMIN);
    gdl90.track_hdg_encode(track_hdg, pos.course);
    GDL90::ADDR_TYPE type = pos.addressType == GATAS::AddressType::ICAO ? GDL90::ADDR_TYPE::ADSB_WITH_ICAO_ADDR : GDL90::ADDR_TYPE::ADSB_WITH_SELF_ADDR;

    GDL90::RawBytes unpacked;
    if (gdl90.ownership_or_traffic_report_encode(
            unpacked,
            false,
            GDL90::ALERT_STATUS::ACTIVE,
            type,
            pos.address,
            latitude,
            longitude,
            altitude,
            GDL90::MISC_TT_HEADING_TRUE_MASK | GDL90::MISC_REPORT_UPDATED_MASK | (pos.groundSpeed > GATAS::GROUNDSPEED_CONSIDERING_AIRBORN ? GDL90::MISC_AIRBORNE_MASK : 0),
            GDL90::NIC::UNKNOWN,
            GDL90::NACP::UNKNOWN, /* Integrity | Accuracy We do not really have this information from the GPS */
            horiz_velocity,
            vert_velocity,
            track_hdg,
            aircraftTypeToEmitter(pos.aircraftType),
            "", // pos.callSign, /* // [0-9A-Z] TODO: Can we use DDB ?? */
            GDL90::EMERGENCY_PRIO::NO_EMERGENCY))
    {
        packAndSend(unpacked);
        statistics.trackingAircraftPosTx += 1;
    }
    else
    {
        statistics.trackingFailureErr += 1;
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
                      (gpsStatusValid ? GDL90::HEARTBEAT_STATUS_GPS_POS_VALID_MASK : 0) |
                      /* GDL90::HEARTBEAT_STATUS_UAT_OK_MASK | */
                      /* GDL90::HEARTBEAT_STATUS_CSA_NOT_AVAIL_MASK | */
                      /*GDL90::HEARTBEAT_STATUS_CSA_REQUESTED_MASK*/
                      0;

    GDL90::RawBytes unpacked;

    // Send heartBeat
    if (gdl90Service.gdl90.heartbeat_encode(unpacked, status, CoreUtils::secondsSinceEpoch() % (24 * 3600), 0, 0))
    {
        gdl90Service.packAndSend(unpacked);
        gdl90Service.statistics.heartbeatTx += 1;
    }
    else
    {
        gdl90Service.statistics.heartBeatEncodingFailureErr += 1;
    }

    // Send ForeFLight heartbeat
    if (gdl90Service.gdl90.foreflight_id_encode(unpacked, 12345, "GaTas", "GaTas Device", 0)) // Bit 0set to 0 Capability WGS-84 ellipsoid bit 0 0 set to 1, MSL
    {
        gdl90Service.packAndSend(unpacked);
        gdl90Service.statistics.heartbeatTx += 1;
    }
    else
    {
        gdl90Service.statistics.heartBeatEncodingFailureErr += 1;
    }
}

void Gdl90Service::packAndSend(const GDL90::RawBytes &unpacked)
{
    GATAS::GdlMsg GdlMsg{};
    gdl90.pack(GdlMsg.msg, unpacked);
    getBus().receive(GdlMsg);
}
