#include <stdint.h>
#include "pico/stdlib.h"
#include <src/lib_crc.hpp>
#include <etl/absolute.h>

#include "flarm_utils.hpp"

uint16_t lonDivisor(float latitude)
{
    constexpr uint16_t table[] =
    {
        52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, // 14
        53, 53, 54, 54, 55, 55, // 6
        56, 56, 57, 57, 58, 58, 59, 59, 60, 60, // 10
        61, 61, 62, 62, 63, 63, 64, 64, 65, 65, // 10
        67, 68, 70, 71, 73, 74, 76, 77, 79, 80, // 10
        82, 83, 85, 86, 88, 89, 91, 94, 98, 101, // 10
        105, 108, 112, 115, 119, 122, 126, 129, 137, 144, // 10
        152, 159, 167, 174, 190, 205, 221, 236, 252, 267, // 10
        299, 330, 362, 425, 489, 552, 616, 679, 743, 806, 806
    }; // 11

    uint8_t ilat = etl::absolute((int)latitude); // TODO: decide if this needs rounding
    if (ilat > 90)
    {
        ilat = 90;
    }

    return table[ilat];
}

uint16_t flarmCalculateChecksum(const uint8_t *flarm_pkt, uint8_t length)
{
    uint16_t crc16 = 0xffff;
    // Add the Flarm address that is not in the packet see:CountryRegulations
    crc16 = update_crc_ccitt(crc16, 0x31);
    crc16 = update_crc_ccitt(crc16, 0xFA);
    crc16 = update_crc_ccitt(crc16, 0xB6);

    for (uint8_t i = 0; i < length; i++)
        crc16 = update_crc_ccitt(crc16, (uint8_t)(flarm_pkt[i]));

    return crc16;
}