#include <stdio.h>

#include "../gdl90service.hpp"

/* GATAS */
#include "ace/coreutils.hpp"
#include "ace/spinlockguard.hpp"

GATAS::PostConstruct Gdl90Service::postConstruct()
{
    return GATAS::PostConstruct::OK;
}

void Gdl90Service::start()
{
    xTaskCreate(gdl90ServiceTask, Gdl90Service::NAME.cbegin(), configMINIMAL_STACK_SIZE + 256, this, tskIDLE_PRIORITY + 2, &taskHandle);
    getBus().subscribe(*this);
};

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
    stream << "}";
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
    // Set UNKNOWN to Airplane because we see a lot of MLAT aircraft without a time
    // Changing this to an aircraft at least does not make it look odd in an EFB
    case GATAS::AircraftCategory::UNKNOWN:
        return GDL90::EMITTER::SMALL;
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

void Gdl90Service::on_receive(const GATAS::ConfigUpdatedMsg &msg)
{
    if (msg.moduleName == Configuration::NAME)
    {
        const Configuration &config = msg.config;
        auto current = config.gaTasConfig();
        auto ownCallSign = config.getCallSignFromHex(current.conspicuity.icaoAddress);
        SPINLOCK_GUARD(CoreUtils::sharedSpinLock());
        ownshipCallsign = makeGdlCallsign(ownCallSign);
    }
}

GATAS::CallSign Gdl90Service::makeGdlCallsign(const GATAS::CallSign &callSign) const
{
    auto ownshipCallsign = callSign;
    bool hasExclamation = false;
    if (ownshipCallsign.size() > 8)
    {
        ownshipCallsign.resize(8);
    }
    for (char &c : ownshipCallsign)
    {
        // Handle the exclamation mark mark as special for now.
        // THis is a NMEA PFLAA indicator send my gatasConnect
        // THis will change in future (gatasConnect) so for now we remove everything
        // after the space
        if (c == '!' || hasExclamation)
        {
            hasExclamation = true;
            c = ' ';
            continue;
        }

        // Lowercase → uppercase
        if (c >= 'a' && c <= 'z')
        {
            c = c - 'a' + 'A';
            continue;
        }

        // Now allowed chars are 0–9 or A–Z
        bool isDigit = (c >= '0' && c <= '9');
        bool isUpper = (c >= 'A' && c <= 'Z');

        if (!isDigit && !isUpper)
        {
            c = '-';
        }
    }
    return ownshipCallsign;
}

void Gdl90Service::on_receive(const GATAS::OwnshipPositionMsg &msg)
{
    const GATAS::OwnshipPositionInfo &pos = msg.position;
    ownshipGeoidSeparation = pos.geoidSeparation;

    uint32_t latitude;
    uint32_t longitude;
    uint32_t altitude;
    uint32_t horiz_velocity;
    uint32_t vert_velocity;
    uint32_t track_hdg;

    auto ownCallSign = SpinlockGuard::copyWithLock(CoreUtils::sharedSpinLock(), ownshipCallsign);

    auto type = pos.conspicuity.addressType == GATAS::AddressType::ICAO ? GDL90::ADDR_TYPE::ADSB_WITH_ICAO_ADDR : GDL90::ADDR_TYPE::ADSB_WITH_SELF_ADDR;

    gdl90.latlon_encode(latitude, pos.lat);
    gdl90.latlon_encode(longitude, pos.lon);
    gdl90.altitude_encode(altitude, pos.heightMsl() * M_TO_FT);
    gdl90.horizontal_velocity_encode(horiz_velocity, pos.groundSpeed * MS_TO_KN);
    gdl90.vertical_velocity_encode(vert_velocity, pos.verticalSpeed * MS_TO_FTPMIN);
    gdl90.track_hdg_encode(track_hdg, pos.course);
    GDL90::RawBytes unpacked;

    // TODO: set UERE based on GPS status, for example when we have SBAS set a lower uere to like 2??
    // Note: These are just estimates!
    // 0:Fix Not Valid 1:GPS fix 2:DGPS SBAS etc.. 3..6:NA
    constexpr float uereDGPS = 2.7f; // Assume a basic UERE of 3 meters
    constexpr float uereGPS = 7.f;   // Assume a basic UERE of 7 meters without DGPS
    auto uere = gpsFix.fixType == GATAS::GpsFixType::DGPS ? uereDGPS : uereGPS;
    float hfom = hDop > 0 ? hDop * uere : -1;
    float hpl = pDop > 0 ? pDop * uere * 2.0f : -1;

    auto nacp = calcNACp(hfom);
    auto nic = calcNIC(hpl);

    if (gdl90.ownership_or_traffic_report_encode(
            unpacked,
            true,
            GDL90::ALERT_STATUS::INACTIVE,
            type,
            pos.conspicuity.icaoAddress,
            latitude,
            longitude,
            altitude, // GDL90 spec states we can set it to 0xFFF if barometric is not available, but SkyDemoon does not like that. So we add this anyways
            // Same as for tracked aircraft, force to AIRBORN unless we can understand how forflight handles this bit
            GDL90::MISC_TT_HEADING_TRUE_MASK | GDL90::MISC_AIRBORNE_MASK,
            // GDL90::MISC_TT_HEADING_TRUE_MASK | (msg.position.airborne ? GDL90::MISC_AIRBORNE_MASK : GDL90::MISC_ON_GROUND_MASK),
            nic,
            nacp,
            horiz_velocity,
            vert_velocity,
            track_hdg,
            aircraftTypeToEmitter(pos.conspicuity.category),
            ownCallSign,
            GDL90::EMERGENCY_PRIO::NO_EMERGENCY))
    {
        packAndSend(unpacked);
        statistics.ownshipPosTx += 1;
    }
    else
    {
        statistics.ownEncodingFailureErr += 1;
    }

    if (gpsFix.hasFix)
    {
        // TODO: Low priority, to validate:
        // Set warning if vertical position accuracy, a bit of a estimate right now, based on not SBAS/WAAS
        // Note to Self, pDOP ==3 is around 20m..30m
        // Note to Self, pDOP ==4 is around 30m..40m
        bool vertical_warning = pDop > 3.0f;
        constexpr float vertical_figure_of_merit_f = 10.f * M_TO_FT;
        uint32_t vertical_figure_of_merit;
        uint32_t geo_altitude;
        bool ok = gdl90.geo_altitude_encode(geo_altitude, pos.ellipseHeight * M_TO_FT);
        ok |= gdl90.vertical_figure_of_merit_encode(vertical_figure_of_merit, vertical_figure_of_merit_f);
        if (ok && gdl90.ownership_geometric_altitude_encode(unpacked, geo_altitude, vertical_warning, vertical_figure_of_merit))
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
    gpsFix = msg.gpsFix;
    pDop = msg.pDop;
    hDop = msg.hDop;
}

GDL90::NACP Gdl90Service::calcNACp(float hfomMeters)
{
    if (hfomMeters <= 0.0f)
    {
        return GDL90::NACP::UNKNOWN;
    }

    // First check meter thresholds (NACp 9–11)
    // clang-format off
    if (hfomMeters < 3.0f)   return GDL90::NACP::HFOM_LT_3_VFOM_LT_4;     // NACp 11
    if (hfomMeters < 10.0f)  return GDL90::NACP::HFOM_LT_10_VFOM_LT_15;   // NACp 10
    if (hfomMeters < 30.0f)  return GDL90::NACP::HFOM_LT_30_VFOM_LT_45;   // NACp 9
    // clang-format on

    // Now convert to NM and check NACp 1–8 thresholds
    const float hfomNm = hfomMeters / 1852.0f;

    // clang-format off
    if (hfomNm < 0.05f) return GDL90::NACP::LT_0_01_NM;
    if (hfomNm < 0.1f)  return GDL90::NACP::LT_0_1_NM;
    if (hfomNm < 0.3f)  return GDL90::NACP::LT_0_3_NM;
    if (hfomNm < 0.5f)  return GDL90::NACP::LT_0_5_NM;
    if (hfomNm < 1.0f)  return GDL90::NACP::LT_1_0_NM;
    if (hfomNm < 2.0f)  return GDL90::NACP::LT_2_0_NM;
    if (hfomNm < 4.0f)  return GDL90::NACP::LT_4_0_NM;
    if (hfomNm < 10.0f) return GDL90::NACP::LT_10_0_NM;
    // clang-format on

    return GDL90::NACP::UNKNOWN;
}

GDL90::NIC Gdl90Service::calcNIC(float hplMeters)
{
    if (hplMeters <= 0.0f)
    {
        return GDL90::NIC::UNKNOWN;
    }

    // First check the meter-based NIC 9..11 thresholds
    // clang-format off
    if (hplMeters < 7.5f)  return GDL90::NIC::HPL_LT_7_5_VPL_LT_11;
    if (hplMeters < 25.0f) return GDL90::NIC::HPL_LT_25_VPL_LT_37_5;
    if (hplMeters < 75.0f) return GDL90::NIC::HPL_LT_75_VPL_LT_112;
    // clang-format on

    // Now convert to nautical miles and check NIC 1..8 thresholds
    const float hplNm = hplMeters / 1852.0f;

    // NM thresholds for NIC 1–8 (increasing)
    // clang-format off
    if (hplNm < 0.1f) return GDL90::NIC::LT_0_1_NM;
    if (hplNm < 0.2f) return GDL90::NIC::LT_0_2_NM;
    if (hplNm < 0.6f) return GDL90::NIC::LT_0_6_NM;
    if (hplNm < 1.0f) return GDL90::NIC::LT_1_0_NM;
    if (hplNm < 2.0f) return GDL90::NIC::LT_2_0_NM;
    if (hplNm < 4.0f) return GDL90::NIC::LT_4_0_NM;
    if (hplNm < 8.0f) return GDL90::NIC::LT_8_0_NM;
    if (hplNm < 20.0f) return GDL90::NIC::LT_20_0_NM;
    // clang-format on

    return GDL90::NIC::UNKNOWN;
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
    gdl90.altitude_encode(altitude, (pos.ellipseHeight - ownshipGeoidSeparation) * M_TO_FT);
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
            // We force this to AIRBORN because foreflight was not shoing GROUND traffic at all.
            // When this is changed back to pos.airborne, we have to know for sure that ForeFLight will show that traffic, even when on the ground
            // Perhaps this was just a setting mistake on my end, not sure. But for now, to have better usability, we force it to AIRBORN
            GDL90::MISC_TT_HEADING_TRUE_MASK | GDL90::MISC_AIRBORNE_MASK,
            // GDL90::MISC_TT_HEADING_TRUE_MASK | (pos.airborne ? GDL90::MISC_AIRBORNE_MASK : GDL90::MISC_ON_GROUND_MASK),
            GDL90::NIC::UNKNOWN,
            GDL90::NACP::UNKNOWN,
            horiz_velocity,
            vert_velocity,
            track_hdg,
            aircraftTypeToEmitter(pos.aircraftType),
            makeGdlCallsign(msg.position.callSign),
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
                      (gpsFix.hasFix ? GDL90::HEARTBEAT_STATUS_GPS_POS_VALID_MASK : 0) |
                      /* GDL90::HEARTBEAT_STATUS_UAT_OK_MASK | */
                      /* GDL90::HEARTBEAT_STATUS_CSA_NOT_AVAIL_MASK | */
                      /* GDL90::HEARTBEAT_STATUS_CSA_REQUESTED_MASK*/
                      0;

    GDL90::RawBytes unpacked;

    // Send heartBeat
    // For the counters, I guess I could take them from tarffic received, is this used at all?
    if (gdl90Service.gdl90.heartbeat_encode(unpacked, status, CoreUtils::secondsSinceEpoch() % (24 * 60 * 60), 0, 0))
    {
        gdl90Service.packAndSend(unpacked);
        gdl90Service.statistics.heartbeatTx += 1;
    }
    else
    {
        gdl90Service.statistics.heartBeatEncodingFailureErr += 1;
    }

    // Send ForeFLight heartbeat
    // https://www.foreflight.com/connect/spec/
    if (gdl90Service.gdl90.foreflight_id_encode(unpacked, 0xace000ace, "GA/TAS", "GA/TAS Conspcty", 0b00)) // Bit 0set to 0 Capability WGS-84 ellipsoid bit 1/2 to 0 for unlimited internet
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
