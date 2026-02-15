#pragma once

#include "trafficpayload.hpp"

namespace ADSL
{

       inline void printfTrafficPayload(const TrafficPayload &p)
       {
              printf("=== ADSL TrafficPayload ===\n");
              printf("Timestamp             : %u qms\n", p.timestampQms());
              printf("Flight state          : 0x%02X -> %s\n", p.flightState().get_enum(), p.flightState().c_str());
              printf("Aircraft category     : 0x%02X -> %s\n", p.aircraftCategory().get_enum(), p.aircraftCategory().c_str());
              printf("Emergency status      : 0x%02X -> %s\n", p.emergencyStatus().get_enum(), p.emergencyStatus().c_str());
              printf("Latitude              : %.6f deg\n", p.latitude());
              printf("Longitude             : %.6f deg\n", p.longitude());
              printf("Ground speed          : %.2f m/s\n", p.speed());
              printf("Ground track          : %.2f deg\n", p.groundTrack());
              printf("Altitude              : %ld m\n", static_cast<long>(p.altitude()));
              printf("Vertical rate         : %.2f m/s\n", p.verticalRate());
              printf("Source integrity lvl  : 0x%02X -> %s\n", p.sourceIntegrityLevel().get_enum(), p.sourceIntegrityLevel().c_str());
              printf("Design assurance      : 0x%02X -> %s\n", p.designAssurance().get_enum(), p.designAssurance().c_str());
              printf("Navigation integrity  : 0x%02X -> %s\n", p.navigationIntegrity().get_enum(), p.navigationIntegrity().c_str());
              printf("Horiz pos accuracy    : 0x%02X -> %s\n", p.horizontalPositionAccuracy().get_enum(), p.horizontalPositionAccuracy().c_str());
              printf("Vert pos accuracy     : 0x%02X -> %s\n", p.verticalPositionAccuracy().get_enum(), p.verticalPositionAccuracy().c_str());
              printf("Velocity accuracy     : 0x%02X -> %s\n", p.velocityAccuracy().get_enum(), p.velocityAccuracy().c_str());
              printf("============================\n");
       }

       inline void printfHeader(const Header &h)
       {
              printf("=== ADSL Header ===\n");
              printf("payloadTypeIdentifier : 0x%02X -> %s\n", h.type().get_enum(), h.type().c_str());
              printf("addressMappingTable   : 0x%02X\n", static_cast<uint8_t>(h.addressMappingTable()));
              printf("address               : 0x%06lX\n", static_cast<unsigned long>(h.address().asUint() & 0xFFFFFF));
              printf("forward (relay)       : 0x%02X\n", static_cast<uint8_t>(h.forward()));
              printf("====================\n");
       }

       inline void printfNetworkPayload(const NetworkPayload &n)
       {
              printf("=== ADSL NetworkPayload ===\n");
              printf("protocolVersion       : 0x%02X -> %s\n", n.protocolVersion().get_enum(), n.protocolVersion().c_str());
              printf("secureSignatureFlag   : 0x%02X\n", static_cast<uint8_t>(n.secureSignatureFlag()));
              printf("keyIndex              : 0x%02X\n", static_cast<uint8_t>(n.keyIndex()));
              printf("errorControlMode      : 0x%02X -> %s\n", n.errorControlMode().get_enum(), n.errorControlMode().c_str());

              printf("===========================\n");
       }

       // template <typename T>
       // inline void printBufferHex(etl::span<T> buffer)
       // {
       //        printf("Length(%d) ", static_cast<int>(buffer.size()));

       //        for (size_t i = 0; i < buffer.size(); ++i)
       //        {
       //               if constexpr (sizeof(T) == 1)
       //                      printf("0x%02" PRIX8, static_cast<uint8_t>(buffer[i]));
       //               else if constexpr (sizeof(T) == 2)
       //                      printf("0x%04" PRIX16, static_cast<uint16_t>(buffer[i]));
       //               else if constexpr (sizeof(T) == 4)
       //                      printf("0x%08" PRIX32, static_cast<uint32_t>(buffer[i]));
       //               else
       //                      printf("0x%X", static_cast<unsigned int>(buffer[i])); // fallback

       //               if (i + 1 < buffer.size())
       //                      printf(", ");
       //        }
       // }
}