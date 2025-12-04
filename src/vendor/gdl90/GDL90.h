// Copyright (c) 2022-2023 Robert A. Alfieri
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
// 
// GDL90.h - single header file implementing GDL90 message encoding and decoding 
//
// Refer to README.md for usage.
//
#ifndef _GDL90_h
#define _GDL90_h

#include <cstdint>
#include <cmath>
#include <string>
#include <etl/array_view.h>
#include <etl/vector.h>
#include <etl/string.h>

class GDL90
{
public:
    GDL90( void );
    ~GDL90();

    // bool set this to true to abort when an error occurs rather than returning false
    bool abort_on_error;

    // Call this once to perform self-testing of these functions.
    // Be sure to check the return value to see if it passed.
    bool     self_test( void );               

    using RawBytes = etl::vector<uint8_t, 48>;

    // pack   - take encoded rawbytes and add escape sequences, CRC, and start/end delimiters so message is ready to send out
    // unpack - inverse; returns false if message is corrupted or has an incorrect format
    bool     pack(         etl::ivector<uint8_t>& packed, const etl::ivector<uint8_t>& unpacked );
    bool     unpack( const etl::ivector<uint8_t>& packed,       etl::ivector<uint8_t>& unpacked );

    // message types
    enum class MESSAGE_ID
    {
        HEARTBEAT                       = 0,
        INITIALIZATION                  = 2,
        UPLINK_DATA                     = 7,
        HEIGHT_ABOVE_TERRAIN            = 9,
        OWNERSHIP_REPORT                = 10,
        OWNERSHIP_GEOMETRIC_ALTITUDE    = 11,
        TRAFFIC_REPORT                  = 20,
        BASIC_UAT_REPORT                = 30,
        LONG_UAT_REPORT                 = 31,

        // popular extensions
        FOREFLIGHT                      = 0x65,
    };
    enum class MESSAGE_FOREFLIGHT_SUBID
    {
        ID                              = 0,
        AHRS                            = 1,
    };    

    // predecode helpers
    bool     id_decode( MESSAGE_ID& id, const etl::ivector<uint8_t>& unpacked );
    bool     foreflight_subid_decode( MESSAGE_FOREFLIGHT_SUBID& subid, const etl::ivector<uint8_t>& unpacked );

    // HEARTBEAT
    static constexpr uint32_t HEARTBEAT_STATUS_UAT_INITIALIZED_MASK = 0x0001;
    static constexpr uint32_t HEARTBEAT_STATUS_RATCS_MASK           = 0x0004;
    static constexpr uint32_t HEARTBEAT_STATUS_GPS_BAT_LOW_MASK     = 0x0008;
    static constexpr uint32_t HEARTBEAT_STATUS_ADDR_TYPE_MASK       = 0x0010;
    static constexpr uint32_t HEARTBEAT_STATUS_IDENT_MASK           = 0x0020;
    static constexpr uint32_t HEARTBEAT_STATUS_MAINT_REQD_MASK      = 0x0040;
    static constexpr uint32_t HEARTBEAT_STATUS_GPS_POS_VALID_MASK   = 0x0080;
    static constexpr uint32_t HEARTBEAT_STATUS_UAT_OK_MASK          = 0x0100;
    static constexpr uint32_t HEARTBEAT_STATUS_CSA_NOT_AVAIL_MASK   = 0x2000;
    static constexpr uint32_t HEARTBEAT_STATUS_CSA_REQUESTED_MASK   = 0x4000;
    static constexpr uint32_t HEARTBEAT_STATUS_ALLOWED_MASK         = HEARTBEAT_STATUS_UAT_INITIALIZED_MASK |
                                                                            HEARTBEAT_STATUS_RATCS_MASK |
                                                                            HEARTBEAT_STATUS_GPS_BAT_LOW_MASK |
                                                                            HEARTBEAT_STATUS_ADDR_TYPE_MASK |
                                                                            HEARTBEAT_STATUS_IDENT_MASK |
                                                                            HEARTBEAT_STATUS_MAINT_REQD_MASK |
                                                                            HEARTBEAT_STATUS_GPS_POS_VALID_MASK |
                                                                            HEARTBEAT_STATUS_UAT_OK_MASK |
                                                                            HEARTBEAT_STATUS_CSA_NOT_AVAIL_MASK |
                                                                            HEARTBEAT_STATUS_CSA_REQUESTED_MASK;
    static constexpr uint32_t HEARTBEAT_STATUS_DISALLOWED_MASK      = ~HEARTBEAT_STATUS_ALLOWED_MASK;

    bool     heartbeat_encode(       etl::ivector<uint8_t>& unpacked, uint32_t  status, uint32_t  timestamp, uint32_t  msg_count_uplink, uint32_t  msg_count_basic_and_long );
    bool     heartbeat_decode( const etl::ivector<uint8_t>& unpacked, uint32_t& status, uint32_t& timestamp, uint32_t& msg_count_uplink, uint32_t& msg_count_basic_and_long );

    // INITIALIZATION
    static constexpr uint32_t INIT_CONFIG_CDTI_OK_MASK              = 0x0001;
    static constexpr uint32_t INIT_CONFIG_AUDIO_INHIBIT_MASK        = 0x0002;
    static constexpr uint32_t INIT_CONFIG_AUDIO_TEST_MASK           = 0x0040;
    static constexpr uint32_t INIT_CONFIG_CSA_DISABLE_MASK          = 0x0100;
    static constexpr uint32_t INIT_CONFIG_CSA_AUDIO_DISABLE_MASK    = 0x0200;
    static constexpr uint32_t INIT_CONFIG_ALLOWED_MASK              = INIT_CONFIG_CDTI_OK_MASK |
                                                                            INIT_CONFIG_AUDIO_INHIBIT_MASK |
                                                                            INIT_CONFIG_AUDIO_TEST_MASK |
                                                                            INIT_CONFIG_CSA_DISABLE_MASK |
                                                                            INIT_CONFIG_CSA_AUDIO_DISABLE_MASK;
    static constexpr uint32_t INIT_CONFIG_DISALLOWED_MASK           = ~INIT_CONFIG_ALLOWED_MASK;

    bool     initialization_encode(       etl::ivector<uint8_t>& unpacked, uint32_t  config );
    bool     initialization_decode( const etl::ivector<uint8_t>& unpacked, uint32_t& config );

    // UPLINK_DATA, BASIC_UAT_REPORT, LONG_UAT_REPORT
    static constexpr uint32_t TIME_OF_RECEPTION_FRAC_ENCODED_INVALID = 0xffffff;
    bool     time_of_reception_frac_encode( uint32_t& frac_encoded, float  frac );  // frac is fractions of a second since the heartbeat timestamp
    bool     time_of_reception_frac_decode( uint32_t  frac_encoded, float& frac );  // frac set to NaN  if frac_encoded==TIME_OF_RECEPTION_FRAC_ENCODED_INVALID

    bool     uplink_data_encode(       etl::ivector<uint8_t>& unpacked, uint32_t  time_of_reception_frac, const etl::array_view<uint8_t>& payload );
    bool     uplink_data_decode( const etl::ivector<uint8_t>& unpacked, uint32_t& time_of_reception_frac,       etl::ivector<uint8_t>& payload );

    bool     basic_uat_report_encode(       etl::ivector<uint8_t>& unpacked, uint32_t  time_of_reception_frac, const etl::array_view<uint8_t>& payload );
    bool     basic_uat_report_decode( const etl::ivector<uint8_t>& unpacked, uint32_t& time_of_reception_frac,       etl::ivector<uint8_t>& payload );

    bool     long_uat_report_encode(       etl::ivector<uint8_t>& unpacked, uint32_t  time_of_reception_frac, const etl::array_view<uint8_t>& payload );
    bool     long_uat_report_decode( const etl::ivector<uint8_t>& unpacked, uint32_t& time_of_reception_frac,       etl::ivector<uint8_t>& payload );


    // OWNERSHIP_REPORT, TRAFFIC_REPORT
    enum class ALERT_STATUS
    {
        INACTIVE                = 0,
        ACTIVE                  = 1,
        __LAST                  = 1,
    };

    enum class ADDR_TYPE
    {
        ADSB_WITH_ICAO_ADDR     = 0,
        ADSB_WITH_SELF_ADDR     = 1,
        TISB_WITH_ICAO_ADDR     = 2,
        TISB_WITH_TRACK_FILE_ID = 3,
        SURFACE_VEHICLE         = 4,
        GROUND_STATION_BEACON   = 5,
        __LAST                  = 5,
    };

    static constexpr uint32_t MISC_TT_NOT_VALID_MASK               = 0b0000;
    static constexpr uint32_t MISC_TT_TRUE_TRACK_ANGLE_MASK        = 0b0001;
    static constexpr uint32_t MISC_TT_HEADING_MAGNETIC_MASK        = 0b0010;
    static constexpr uint32_t MISC_TT_HEADING_TRUE_MASK            = 0b0011;
    static constexpr uint32_t MISC_REPORT_UPDATED_MASK             = 0b0000;
    static constexpr uint32_t MISC_REPORT_EXTRAPOLATED_MASK        = 0b0100;
    static constexpr uint32_t MISC_ON_GROUND_MASK                  = 0b0000;
    static constexpr uint32_t MISC_AIRBORNE_MASK                   = 0b1000;
    static constexpr uint32_t MISC_ALLOWED_MASK                    = 0b1111;

    enum class NIC
    {
        UNKNOWN                 = 0,
        LT_20_0_NM              = 1,
        LT_8_0_NM               = 2,
        LT_4_0_NM               = 3,
        LT_2_0_NM               = 4,
        LT_1_0_NM               = 5,
        LT_0_6_NM               = 6,
        LT_0_2_NM               = 7,
        LT_0_1_NM               = 8,
        HPL_LT_75_VPL_LT_112    = 9,
        HPL_LT_25_VPL_LT_37_5   = 10,
        HPL_LT_7_5_VPL_LT_11    = 11,
        __LAST                  = 11,
    };

    enum class NACP
    {
        UNKNOWN                 = 0,
        LT_10_0_NM              = 1,
        LT_4_0_NM               = 2,
        LT_2_0_NM               = 3,
        LT_1_0_NM               = 4,
        LT_0_5_NM               = 5,
        LT_0_3_NM               = 6,
        LT_0_1_NM               = 7,
        LT_0_01_NM              = 8,
        HFOM_LT_30_VFOM_LT_45   = 9,
        HFOM_LT_10_VFOM_LT_15   = 10,
        HFOM_LT_3_VFOM_LT_4     = 11,
        __LAST                  = 11,
    };

    enum class EMITTER
    {
        UNKNOWN                 = 0,
        LIGHT                   = 1,
        SMALL                   = 2,
        LARGE                   = 3,
        HIGH_VORTEX_LARGE       = 4,
        HEAVY                   = 5,
        HIGHLY_MANEUVERABLE     = 6,
        ROTOCRAFT               = 7,
        GLIDER_SAILPLANE        = 8,
        LIGHTER_THAN_AIR        = 10,
        PARACHUTIST             = 11,
        ULTRA_LIGHT             = 12,
        UNMANNED                = 14,
        SPACE                   = 15,
        SURFACE_EMERGENCY       = 17,
        SURFACE_SERVICE         = 18,
        POINT_OBSTACLE          = 19,
        CLUSTER_OBSTACLE        = 20,
        LINE_OBSTACLE           = 21,
        __LAST                  = 21,
    };

    enum class EMERGENCY_PRIO
    {
        NO_EMERGENCY            = 0,
        GENERAL_EMERGENCY       = 1,
        MEDICAL_EMERGENCY       = 2,
        MINIMUM_FUEL            = 3,
        NO_COMMUNICATION        = 4,
        UNLAWFUL_INTERFERENCE   = 5,
        DOWNED_AIRCRAFT         = 6,
        __LAST                  = 6,
    };

    bool     latlon_encode( uint32_t& latlon_encoded, float  latlon );  // latlon must be -180.0 .. 180.0(minus LSB epsilon); north and east are considered positive
    bool     latlon_decode( uint32_t  latlon_encoded, float& latlon );  

    static constexpr uint32_t ALTITUDE_ENCODED_INVALID                = 0xfff;  
    bool     altitude_encode( uint32_t& altitude_encoded, float  altitude );  // altitude must be -1000 ft to +101,350 ft
    bool     altitude_decode( uint32_t  altitude_encoded, float& altitude );  // altitude is set to NaN if altitude_encoded==ALTITUDE_ENCODED_INVALID

    static constexpr uint32_t HORIZONTAL_VELOCITY_ENCODED_INVALID          = 0xfff;  
    bool     horizontal_velocity_encode( uint32_t& velocity_encoded, float  velocity );  // valid range is 0 .. 4094 knots; if exceeded, then it is hammered to 4094
    bool     horizontal_velocity_decode( uint32_t  velocity_encoded, float& velocity );  // velocity is set to NaN if velocity_encoded==HORIZONTAL_VELOCITY_ENCODED_INVALID

    static constexpr uint32_t VERTICAL_VELOCITY_ENCODED_INVALID          = 0x800;
    bool     vertical_velocity_encode( uint32_t& velocity_encoded, float  velocity );  // valid range is +/- 32,576 FPM; if exceeded, then it is hammered to +/- 32,640
    bool     vertical_velocity_decode( uint32_t  velocity_encoded, float& velocity );  // velocity is set to NaN if velocity_encoded==VERTICAL_VELOCITY_ENCODED_INVALID

    bool     track_hdg_encode( uint32_t& track_hdg_encoded, float  track_hdg );        // valid range is 0-360 deg
    bool     track_hdg_decode( uint32_t  track_hdg_encoded, float& track_hdg );        

    bool     is_valid_call_sign( const etl::string_view call_sign );

    bool     ownership_or_traffic_report_encode(       etl::ivector<uint8_t>& unpacked, bool is_ownership, ALERT_STATUS alert_status, ADDR_TYPE addr_type, uint32_t participant_address, 
                                                                                 uint32_t latitude, uint32_t longitude, uint32_t altitude, uint32_t misc, 
                                                                                 NIC nic, NACP nacp, uint32_t horiz_velocity, uint32_t vert_velocity, uint32_t track_hdg, 
                                                                                 EMITTER emitter, const etl::string_view call_sign, EMERGENCY_PRIO emergency_prio_code );
    bool     ownership_or_traffic_report_decode( const etl::ivector<uint8_t>& unpacked, bool is_ownership, ALERT_STATUS& alert_status, ADDR_TYPE& addr_type, uint32_t& participant_address, 
                                                                                 uint32_t& latitude, uint32_t& longitude, uint32_t& altitude, uint32_t& misc, 
                                                                                 NIC& nic, NACP& nacp, uint32_t& horiz_velocity, uint32_t& vert_velocity, uint32_t& track_hdg, 
                                                                                 EMITTER& emitter, etl::istring & call_sign, EMERGENCY_PRIO& emergency_prio_code );

    // HEIGHT_ABOVE_TERRAIN
    static constexpr uint32_t HEIGHT_ENCODED_INVALID = 0x80000;
    bool     height_encode( uint32_t& height_encoded, float  height );  // height must be -32767 .. 32767; if NaN, height_encoded is set to HEIGHT_ENCODED_INVALID
    bool     height_decode( uint32_t  height_encoded, float& height );  // height is set to NaN if height_encoded==HEIGHT_ENCODED_INVALID

    bool     height_above_terrain_encode(       etl::ivector<uint8_t>& unpacked, uint32_t  height );  
    bool     height_above_terrain_decode( const etl::ivector<uint8_t>& unpacked, uint32_t& height );  
    
    // OWNERSHIP_GEOMETRIC_ALTITUDE
    bool     geo_altitude_encode( uint32_t& geo_altitude_encoded, float  geo_altitude );  // geo_altitude must be -5*32768 .. 5*32767; 
    bool     geo_altitude_decode( uint32_t  geo_altitude_encoded, float& geo_altitude );  

    static constexpr uint32_t VERTICAL_FIGURE_OF_MERIT_NOT_AVAIL = 0x7fff;
    static constexpr uint32_t VERTICAL_FIGURE_OF_MERIT_GE_32766  = 0x7ffe;
    bool     vertical_figure_of_merit_encode( uint32_t& vertical_figure_of_merit_encoded, float  vertical_figure_of_merit ); // must be in meters and >= 0; NaN => NOT_AVAIL
    bool     vertical_figure_of_merit_decode( uint32_t  vertical_figure_of_merit_encoded, float& vertical_figure_of_merit ); // set to NaN if VERTICAL_FIGURE_OF_MERIT_NOT_AVAIL

    bool     ownership_geometric_altitude_encode(       etl::ivector<uint8_t>& unpacked, uint32_t  geo_altitude, bool  vertical_warning, uint32_t  vertical_figure_of_merit );
    bool     ownership_geometric_altitude_decode( const etl::ivector<uint8_t>& unpacked, uint32_t& geo_altitude, bool& vertical_warning, uint32_t& vertical_figure_of_merit );

    // FOREFLIGHT ID
    static constexpr uint32_t FOREFLIGHT_CAPABILITIES_GEO_ALTITUDE_USED_MASK = 0x00000001; // in OWNERSHIP_GEOMETRIC_ALTITUDE message
    static constexpr uint32_t FOREFLIGHT_CAPABILITIES_WGS84_ELLIPSOID_MASK   = 0x00000002; // per GDL90 spec
    static constexpr uint32_t FOREFLIGHT_CAPABILITIES_ALLOWED_MASK           = FOREFLIGHT_CAPABILITIES_GEO_ALTITUDE_USED_MASK |
                                                                               FOREFLIGHT_CAPABILITIES_WGS84_ELLIPSOID_MASK;
    static constexpr uint32_t FOREFLIGHT_CAPABILITIES_DISALLOWED_MASK        = ~FOREFLIGHT_CAPABILITIES_ALLOWED_MASK;

    static constexpr uint64_t FOREFLIGHT_DEVICE_SERIAL_NUMBER_INVALID = 0xffffffffffffffffULL;

    bool     foreflight_id_encode(       etl::ivector<uint8_t>& unpacked, uint64_t  device_serial_number, const etl::string_view device_name, const etl::string_view device_long_name, uint32_t  capabilities_mask );
    bool     foreflight_id_decode( const etl::ivector<uint8_t>& unpacked, uint64_t& device_serial_number, etl::istring & device_name, etl::istring & device_long_name, uint32_t& capabilities_mask );

    // FOREFLIGHT AHRS
    static constexpr uint32_t FOREFLIGHT_ROLL_PITCH_INVALID = 0x7fff;
    bool     foreflight_roll_pitch_encode( uint32_t& roll_pitch_encoded, float  roll_pitch );  // roll/pitch must be -180.0 .. 180.0; NaN => FOREFLIGHT_ROLL_PITCH_INVALID
    bool     foreflight_roll_pitch_decode( uint32_t  roll_pitch_encoded, float& roll_pitch );  // if FOREFLIGHT_ROLL_PITCH_INVALID, roll_pitch will be set to NaN

    static constexpr uint32_t FOREFLIGHT_HEADING_INVALID = 0xffff;
    bool     foreflight_heading_encode( uint32_t& heading_encoded, float  heading, bool  is_magnetic );  // heading must be -360.0 .. 360.0; NaN => FOREFLIGHT_HEADING_INVALID
    bool     foreflight_heading_decode( uint32_t  heading_encoded, float& heading, bool& is_magnetic );  // if FOREFLIGHT_HEADING_INVALID, heading will be set to NaN

    static constexpr uint32_t FOREFLIGHT_AIRSPEED_INVALID = 0xffff;

    bool     foreflight_ahrs_encode(       etl::ivector<uint8_t>& unpacked, uint32_t  roll, uint32_t  pitch, uint32_t  heading, uint32_t  ias, uint32_t  tas );
    bool     foreflight_ahrs_decode( const etl::ivector<uint8_t>& unpacked, uint32_t& roll, uint32_t& pitch, uint32_t& heading, uint32_t& ias, uint32_t& tas );

private:
    uint16_t crc_table[256]; 
    void     crc_init( void );                            // computes crc_table once
    uint16_t crc_compute( const etl::ivector<uint8_t>& unpacked, size_t length );

    inline bool error( )
    {
        if ( abort_on_error ) {
            // std::cout << "ERROR: " << msg << "\n";
            exit( 1 );
        }
        return false;
    }
};

#endif
