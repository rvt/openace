#pragma once

#include "etl/array.h"
#include "etl/span.h"
#include <cstdint>
#include <algorithm>

#include "lib_crc.hpp"

class Flarm2024Packet
{
    static constexpr etl::array<uint32_t, 4> BTEA_KEYS = {0xa5f9b21c, 0xab3f9d12, 0xc6f34e34, 0xd72fa378};
    static constexpr uint32_t BTEA_DELTA = 0x9e3779b9;
    static constexpr uint32_t SCRAMBLE = 0x956f6c77;
    static constexpr uint8_t BTEA_N = 4;
    static constexpr uint8_t BTEA_ROUNDS = 6;

// Based on https://github.com/creaktive/flare/blob/master/flarm_decode.c

#pragma pack(push, 4)
    struct RadioPacket
    {
        uint32_t aircraftIDRaw : 24;           // Bits 0-23: aircraft ID
        uint8_t messageTypeRaw : 4;            // Bits 24-27: message type
        uint8_t addressTypeRaw : 3;            // Bits 28-30: address (ID) type
        uint32_t reserved1Raw : 23;            // Bits 31-53: always 0
        bool stealthRaw : 1;                   // Bit 54: stealth flag
        bool noTrackRaw : 1;                   // Bit 55: no-track flag
        uint8_t reserved3Raw : 4;              // Bits 56-57: always 1
        uint8_t reserved4Raw : 4;              // Bits 58-59: always 0
        uint8_t reserved5Raw : 2;              // Bits 60-61: always 1
        uint8_t flarmTimestampLSBRaw : 4;      // Bits 66-69: FLARM timestamp LSB
        uint8_t aircraftTypeRaw : 4;           // Bits 70-73: aircraft type
        uint8_t reserved7Raw : 1;              // Bit 74: always 0
        uint16_t altitudeRaw : 13;             // Bits 75-87: altitude in meters + 1000, enscaled(12,1)
        uint32_t latitudeRaw : 20;             // Bits 88-107: latitude (rounded and with MS bits removed)
        uint32_t longitudeRaw : 20;            // Bits 108-127: longitude (rounded and with MS bits removed)
        uint16_t turnRateRaw : 9;              // Bits 128-136: turn rate, degs/sec times 20, enscaled(6,2), signed
        uint16_t groundSpeedRaw : 10;          // Bits 137-146: horizontal speed, m/s times 10, enscaled(8,2)
        uint16_t verticalSpeedRaw : 9;         // Bits 147-155: vertical speed, m/s times 10, enscaled(6,2), signed
        uint16_t groundTrackRaw : 10;               // Bits 156-165: groundTrack direction, degrees (0-360) times 2
        uint8_t movementStatusRaw : 2;         // Bits 166-167: 2-bit integer for movement status
        uint8_t gnssHorizontalAccuracyRaw : 6; // Bits 168-173: GNSS horizontal accuracy, meters times 10, enscaled(3,3)
        uint8_t gnssVerticalAccuracyRaw : 5;   // Bits 174-178: GNSS vertical accuracy, meters times 4, enscaled(2,3)
        uint8_t unknownDataRaw : 5;            // Bits 179-183: unknown data
        uint8_t reserved8Raw : 8;              // Bits 184-191: always 0

        uint16_t checksum;
    } __attribute__((packed));
#pragma pack(pop)

public:
    struct LatLon
    {
        float latitude;
        float longitude;
    };
    static constexpr uint8_t TOTAL_LENGTH = 24 + 2;                     // Packet length with CRC
    static constexpr uint8_t TOTAL_LENGTH_WORDS = TOTAL_LENGTH / 4 + 1; // Packet length with CRC

private:
    RadioPacket packet;

public:
    // -------------------- Getters / Setters (add more below as needed) ----------------------
    Flarm2024Packet()
    {
        packet.reserved1Raw = 0;
        packet.reserved3Raw = 0b11;
        packet.reserved4Raw = 0b11;
        packet.reserved5Raw = 0x00;
        packet.unknownDataRaw = 11;
        packet.reserved8Raw = 0x00;
    }

    /**
     * @brief Self test decripting packet structure. Use this to test your platform
     *
     * @return int8_t value from loadFromBuffer, or -5 if the lat/lon are not as expected
     */
    static int8_t selfCheck()
    {
        assert(sizeof(RadioPacket) == 28); // It's 28 because of word alignment
        Flarm2024Packet packet;
        uint32_t epoch = 1751789240;
        uint32_t data[TOTAL_LENGTH_WORDS] = {0x12123456, 0x33000000, 0xBD018364, 0x70F0D201, 0x62ED4B2E, 0xB6507683, 0x0000E31C};

        auto lfbRet = packet.loadFromBuffer(epoch, {data, TOTAL_LENGTH_WORDS});
        auto pos = packet.getPosition(53, 5);
        if (lfbRet != 0)
        {
            return lfbRet;
        }
        else if (pos.latitude - 52.314239 > 0.001 || pos.longitude - 4.754224 > 0.001)
        {
            return -5;
        }

        // Generate the data again to test if both are the same
        uint32_t newData[TOTAL_LENGTH_WORDS] = {};
        packet.writeToBuffer(epoch, newData);

        for (auto i = 0; i < TOTAL_LENGTH_WORDS; i++)
        {
            if (data[i] != newData[i])
            {
                printf("!!! %8X %8X ", data[i], newData[i]);
                return -6;
            }
        }

        return 0;
    }

    uint32_t aircraftId() const
    {
        return packet.aircraftIDRaw;
    }

    void aircraftId(uint32_t aircraftId)
    {
        packet.aircraftIDRaw = aircraftId & 0xFFFFFF;
    }

    uint8_t messageType() const
    {
        return packet.messageTypeRaw;
    }

    void messageType(uint8_t messageType)
    {
        packet.messageTypeRaw = messageType;
    }

    uint8_t addressType() const
    {
        return packet.addressTypeRaw;
    }

    void addressType(uint8_t addressType)
    {
        packet.addressTypeRaw = addressType <= 2 ? addressType : 0;
    }

    bool stealth() const
    {
        return packet.stealthRaw;
    }

    void stealth(bool stealth)
    {
        packet.stealthRaw = stealth;
    }

    bool noTrack() const
    {
        return packet.noTrackRaw;
    }

    void noTrack(bool noTrack)
    {
        packet.noTrackRaw = noTrack;
    }

    uint8_t epochSecondsLSB() const
    {
        return packet.flarmTimestampLSBRaw;
    }

    void epochSeconds(uint32_t epochSeconds)
    {
        packet.flarmTimestampLSBRaw = epochSeconds & 0x0F;
    }

    uint8_t aircraftType() const
    {
        return packet.aircraftTypeRaw;
    }

    void aircraftType(uint8_t aircraftType)
    {
        packet.aircraftTypeRaw = aircraftType > 0b1111 ? 0b1111 : aircraftType;
        ;
    }

    int16_t altitude() const
    {
        return descale<12, 1, false>(packet.altitudeRaw) - 1000;
    }

    void altitude(int16_t meters)
    {
        meters = meters < -1000 ? -1000 : meters;
        meters = meters > 11286 ? 11286 : meters;
        packet.altitudeRaw = enscale<12, 1, false>(std::max<int16_t>(0, static_cast<int16_t>(meters + 1000)));
    }

    void setPosition(float latitude, float longitude)
    {
        if (latitude < 0.f)
        {
            packet.latitudeRaw = (uint32_t)(-(((int32_t)(-latitude * 1e7) + 26.f) / 52.f)) & 0x0FFFFF;
        }
        else
        {
            packet.latitudeRaw = (uint32_t)((((uint32_t)(latitude * 1e7) + 26.f) / 52.f)) & 0x0FFFFF;
        }

        auto divisor = lonDivisor(latitude);
        if (longitude < 0.f)
        {
            packet.longitudeRaw = (uint32_t)(-(((int32_t)(-longitude * 1e7) + (divisor >> 1)) / divisor)) & 0x0FFFFF;
        }
        else
        {
            packet.longitudeRaw = (((uint32_t)(longitude * 1e7) + (divisor >> 1)) / divisor) & 0x0FFFFF;
        }
    }

    LatLon getPosition(float ownLatitude, float ownLongitude) const
    {
        // Calculate rounded latitude
        int32_t ownLatInt = static_cast<int32_t>(ownLatitude * 1e7);
        int32_t round_lat = (ownLatInt + (ownLatInt < 0 ? -26 : 26)) / 52;

        // Adjust latitude
        int32_t ilat = (packet.latitudeRaw - round_lat) & 0x0FFFFF;
        if (ilat >= 0x080000)
        {
            ilat -= 0x100000;
        }
        float aircraftLat = static_cast<float>((ilat + round_lat) * 52) * 1e-7f;

        // Calculate rounded longitude
        int lonDiv = lonDivisor(aircraftLat);
        int32_t ownLonInt = static_cast<int32_t>(ownLongitude * 1e7);
        int32_t round_lon = (ownLonInt + (ownLonInt < 0 ? -(lonDiv >> 1) : (lonDiv >> 1))) / lonDiv;

        // Adjust longitude
        int32_t ilon = (packet.longitudeRaw - round_lon) & 0x0FFFFF;
        if (ilon >= 0x080000)
        {
            ilon -= 0x0100000;
        }
        float aircraftLon = static_cast<float>((ilon + round_lon) * lonDiv) * 1e-7f;

        return {aircraftLat, aircraftLon};
    }

    float turnRate() const
    {
        return descale<6, 2, true>(packet.turnRateRaw) / 20.0f;
    }

    void turnRate(float degPerSec)
    {
        packet.turnRateRaw = enscale<6, 2, true>(degPerSec * 20.0f);
    }

    float groundSpeed() const
    {
        return descale<8, 2, false>(packet.groundSpeedRaw) / 10.0f;
    }

    void groundSpeed(float mps)
    {
        mps = mps < 0.0f ? 0.0f : mps;
        packet.groundSpeedRaw = enscale<8, 2, false>(mps * 10.0f);
    }

    float verticalSpeed() const
    {
        return descale<6, 2, true>(packet.verticalSpeedRaw) / 10.0f;
    }

    void verticalSpeed(float mps)
    {
        packet.verticalSpeedRaw = enscale<6, 2, true>(mps * 10.0f);
    }

    float groundTrack() const
    {
        return (float)(packet.groundTrackRaw) / 2.f;
    }

    void groundTrack(float deg)
    {
        int16_t iDeg = deg * 2;
        if (iDeg < 0)
        {
            iDeg += 720;
        }
        else if (iDeg >= 720)
        {
            iDeg -= 720;
        }

        packet.groundTrackRaw = iDeg;
    }

    uint8_t movementStatus() const
    {
        return packet.movementStatusRaw;
    }

    void movementStatus(uint8_t movementStatus)
    {
        packet.movementStatusRaw = movementStatus > 0b11 ? 0b11 : movementStatus;
    }

    // -------------------- Buffer IO ----------------------

    /**
     * @brief Load packet from buffer. Once the CRC is matched, the result is destructive for the buffer,
     * even when the message was incorrect.
     *
     * @param epochSeconds
     * @param receivedPacket
     * @return int8_t
     */
    int8_t loadFromBuffer(uint32_t epochSeconds, etl::span<const uint32_t> buffer)
    {
        if (buffer.size() != TOTAL_LENGTH_WORDS)
        {
            // Length must be 7 words
            return -2;
        }
        uint16_t calculatedChecksum = flarmCalculateChecksum(reinterpret_cast<const uint8_t *>(buffer.data()), TOTAL_LENGTH - 2); // -2 becayse we do not want to calculate the CRC

        if ((swapBytes16(buffer[6] & 0xFFFF)) != calculatedChecksum)
        {
            // Invalid Checksum
            return -1;
        }

        // Algorithm to find a possible epochTime where the received packed was scrambled at
        // It currently assumes that only when the epochSec & 0x0F rolls over there is a potential
        // This assumes the received packages is always send 'back' in time, echt current Epoch is always > epoch where
        // the packages was scrambled at.
        for (auto offset : {0, -1, -2})
        {
            epochSeconds += offset;
            memcpy(&packet, buffer.data(), TOTAL_LENGTH);
            bteaDecode(((uint32_t *)&packet) + 2);
            scramble(((uint32_t *)&packet), epochSeconds);

            if (packet.flarmTimestampLSBRaw == (epochSeconds & 0x0F))
            {
                return 0;
            }
        }
        return -3;
    }

    int8_t writeToBuffer(uint32_t epochSeconds, etl::span<uint32_t> buffer)
    {
        if (buffer.size() != TOTAL_LENGTH_WORDS)
        {
            // Length must be 7 words
            return -2;
        }

        memcpy(buffer.data(), &packet, TOTAL_LENGTH);
        scramble(buffer.data(), epochSeconds);
        bteaEncode(buffer.data() + 2);

        uint16_t calculatedChecksum = flarmCalculateChecksum(reinterpret_cast<const uint8_t *>(buffer.data()), TOTAL_LENGTH - 2); // -2 because we do not want to calculate the CRC
        buffer[6] = swapBytes16(calculatedChecksum);

        return 0;
    }

    // void loadFromBuffer(etl::span<const uint8_t> buffer)
    // {
    //   etl::bit_stream_reader reader((uint8_t *)buffer.data(), buffer.size(), etl::endian::big);

    //   aircraftIDRaw = etl::reverse_bytes(reader.read_unchecked<uint32_t>(24U) >> 8);
    //   messageTypeRaw = reader.read_unchecked<uint8_t>(4U);
    //   addressTypeRaw = reader.read_unchecked<uint8_t>(3U);
    //   reader.read_unchecked<uint32_t>(23U);
    //   stealthRaw = reader.read_unchecked<bool>();
    //   noTrackRaw = reader.read_unchecked<bool>();
    //   reader.read_unchecked<uint32_t>(10);
    //   flarmTimestampLSBRaw = reader.read_unchecked<uint8_t>(4);
    //   aircraftTypeRaw = reader.read_unchecked<uint8_t>(4);
    //   reader.read_unchecked<uint8_t>(1);
    //   altitudeRaw = reader.read_unchecked<uint16_t>(13);
    //   latitudeRaw = reader.read_unchecked<uint32_t>(20);
    //   longitudeRaw = reader.read_unchecked<uint32_t>(20);

    //   turnRateRaw = reader.read_unchecked<uint16_t>(9);
    //   groundSpeedRaw = reader.read_unchecked<uint16_t>(10);
    //   verticalSpeedRaw = reader.read_unchecked<uint16_t>(9);
    //   groundTrackRaw = reader.read_unchecked<uint16_t>(10);
    //   movementStatusRaw = reader.read_unchecked<uint8_t>(2);
    //   gnssHorizontalAccuracyRaw = reader.read_unchecked<uint8_t>(6);
    //   gnssVerticalAccuracyRaw = reader.read_unchecked<uint8_t>(5);
    //   reader.read_unchecked<uint32_t>(13);
    // }

    // void writeToBuffer(etl::span<const uint8_t> buffer)
    // {
    //   etl::bit_stream_writer writer((uint8_t *)buffer.data(), buffer.size(), etl::endian::big);

    //   //    writer.write_unchecked(etl::reverse_bytes<uint32_t>(aircraftIDRaw << 8), 24U); // ok
    //   writer.write_unchecked(reverseBits(aircraftIDRaw) >> 8, 24U); // ok
    //   writer.write_unchecked(reverseBits(messageTypeRaw) >> 4, 4U); // SHows up as 2
    //   writer.write_unchecked(reverseBits(addressTypeRaw) >> 5, 3U); //
    //   writer.write_unchecked(0, 23U);

    //   writer.write_unchecked(stealthRaw);
    //   writer.write_unchecked(noTrackRaw);

    //   writer.write_unchecked(reverseBits((uint8_t)0b11 ) >> 4, 4U); // 0x3
    //   writer.write_unchecked(reverseBits((uint8_t)0b11 ) >> 4, 4U); // 0x3
    //   writer.write_unchecked(0x00, 2U); // 0x00
    //   writer.write_unchecked(reverseBits(flarmTimestampLSBRaw) >> 4, 4U);
    //   writer.write_unchecked(reverseBits(aircraftTypeRaw) >> 4, 4U);
    //   writer.write_unchecked(0x00, 1U);
    //   writer.write_unchecked(reverseBits(altitudeRaw) >> 3, 13U);
    //   writer.write_unchecked(reverseBits(latitudeRaw) >> 12, 20U);
    //   writer.write_unchecked(reverseBits(longitudeRaw) >> 12, 20U);
    //   writer.write_unchecked(reverseBits(turnRateRaw) >> 7, 9U);
    //   writer.write_unchecked(reverseBits(groundSpeedRaw) >> 6, 10U);
    //   writer.write_unchecked(reverseBits(verticalSpeedRaw) >> 7, 9U);
    //   writer.write_unchecked(reverseBits(groundTrackRaw) >> 6, 10U);
    //   writer.write_unchecked(reverseBits(movementStatusRaw) >> 6, 2U);
    //   writer.write_unchecked(reverseBits(gnssHorizontalAccuracyRaw) >> 2, 6U);
    //   writer.write_unchecked(reverseBits(gnssVerticalAccuracyRaw) >> 3, 5U);
    //   writer.write_unchecked(0, 5U);
    //   writer.write_unchecked(0, 8U);
    // }

    // -------------------- Utils --------------------------
    uint16_t lonDivisor(float latitude) const
    {
        constexpr uint16_t table[] =
            {
                52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, // 14
                53, 53, 54, 54, 55, 55,                                 // 6
                56, 56, 57, 57, 58, 58, 59, 59, 60, 60,                 // 10
                61, 61, 62, 62, 63, 63, 64, 64, 65, 65,                 // 10
                67, 68, 70, 71, 73, 74, 76, 77, 79, 80,                 // 10
                82, 83, 85, 86, 88, 89, 91, 94, 98, 101,                // 10
                105, 108, 112, 115, 119, 122, 126, 129, 137, 144,       // 10
                152, 159, 167, 174, 190, 205, 221, 236, 252, 267,       // 10
                299, 330, 362, 425, 489, 552, 616, 679, 743, 806, 806}; // 11

        uint8_t ilat = etl::absolute((int)latitude); // TODO: decide if this needs rounding
        if (ilat > 90)
        {
            ilat = 90;
        }

        return table[ilat];
    }

    inline uint32_t MX(uint32_t z, uint32_t y, uint32_t sum, uint8_t e, uint8_t p, const etl::span<const uint32_t> &keys) const
    {
        return (((z >> 5) ^ (y << 2)) + ((y >> 3) ^ (z << 4))) ^ ((sum ^ y) + (keys[(p & 3) ^ e] ^ z));
    }

    void bteaEncode(uint32_t *data) const
    {
        auto rounds = BTEA_ROUNDS;
        uint32_t sum = 0;
        uint32_t z = data[BTEA_N - 1];
        do
        {
            uint32_t y;
            sum += BTEA_DELTA;
            uint8_t e = (sum >> 2) & 3;
            for (uint8_t p = 0; p < BTEA_N - 1; p++)
            {
                y = data[p + 1];
                z = data[p] += MX(z, y, sum, e, p, BTEA_KEYS);
            }
            y = data[0];
            z = data[BTEA_N - 1] += MX(z, y, sum, e, BTEA_N - 1, BTEA_KEYS);
        } while (--rounds);
    }

    void bteaDecode(uint32_t *data)
    {
        auto rounds = BTEA_ROUNDS;
        uint32_t sum = rounds * BTEA_DELTA;
        uint32_t y = data[0];
        do
        {
            uint32_t z;
            uint8_t e = (sum >> 2) & 3;
            for (uint8_t p = BTEA_N - 1; p > 0; p--)
            {
                z = data[p - 1];
                y = data[p] -= MX(z, y, sum, e, p, BTEA_KEYS);
            }
            z = data[BTEA_N - 1];
            y = data[0] -= MX(z, y, sum, e, 0, BTEA_KEYS);
            sum -= BTEA_DELTA;
        } while (--rounds);
    }

    void scramble(uint32_t *data, uint32_t timestamp)
    {
        constexpr uint8_t numKeys = 4;
        constexpr uint8_t numIterations = 2;
        constexpr uint8_t byteLength = numKeys * sizeof(uint32_t);

        uint32_t wkeys[] = {data[0], data[1], timestamp >> 4, SCRAMBLE};
        uint8_t *bkeys = reinterpret_cast<uint8_t *>(wkeys);

        uint16_t y, x;
        uint8_t z = bkeys[byteLength - 1];
        uint32_t sum = 0;

        for (auto q = 0; q < numIterations; ++q)
        {
            sum += BTEA_DELTA;
            y = bkeys[0];
            for (uint8_t p = 0; p < byteLength - 1; ++p)
            {
                x = y;
                y = bkeys[p + 1];
                x += ((((z >> 5) ^ (y << 2)) + ((y >> 3) ^ (z << 4))) ^ (sum ^ y));
                bkeys[p] = static_cast<uint8_t>(x);
                z = x & 0xff;
            }
            x = y;
            y = bkeys[0];
            x += ((((z >> 5) ^ (y << 2)) + ((y >> 3) ^ (z << 4))) ^ (sum ^ y));
            bkeys[byteLength - 1] = static_cast<uint8_t>(x);
            z = x & 0xff;
        }

        data[2 + 0] ^= wkeys[0];
        data[2 + 1] ^= wkeys[1];
        data[2 + 2] ^= wkeys[2];
        data[2 + 3] ^= wkeys[3];
    }

    template <uint8_t mbits, uint8_t ebits, uint8_t sbits>
    unsigned int enscale(int value) const
    {
        unsigned int offset = (1 << mbits);
        unsigned int signbit = (offset << ebits);
        unsigned int negative = 0;
        if (value < 0)
        {
            if (!sbits)
            {
                // underflow
                return 0; // clamp to minimum
            }
            value = -value;
            negative = signbit;
        }
        if (static_cast<unsigned int>(value) < offset)
        {
            return (negative | static_cast<unsigned int>(value)); // just for efficiency
        }
        unsigned int exp = 0;
        unsigned int mantissa = offset + static_cast<unsigned int>(value);
        unsigned int mlimit = offset + offset - 1;
        unsigned int elimit = signbit - 1;
        while (mantissa > mlimit)
        {
            mantissa >>= 1;
            exp += offset;
            if (exp > elimit)
            {
                // overflow
                return (negative | elimit); // clamp to maximum
            }
        }
        mantissa -= offset;
        return (negative | exp | mantissa);
    }

    template <uint8_t mbits, uint8_t ebits, bool sbits>
    int descale(unsigned int value) const
    {
        unsigned int offset = (1 << mbits);
        unsigned int signbit = (offset << ebits);
        unsigned int negative = 0;
        if (sbits)
        {
            negative = (value & signbit);
        }
        value &= (signbit - 1); // ignores signbit and higher
        if (value >= offset)
        {
            unsigned int exp = value >> mbits;
            value &= (offset - 1);
            value += offset;
            value <<= exp;
            value -= offset;
        }
        return (negative ? -(int)value : value);
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

    inline uint16_t swapBytes16(uint16_t value)
    {
        return (value >> 8) | (value << 8);
    }
};
