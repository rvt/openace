
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

// #define private public

// // #include <stdio.h>

// // #include "pico/rand.h"
// // #include "pico/time.h"
// // #include "geomock.hpp"
// // #include "mockconfig.h"

// // #include "flarm_utils.hpp"
// #include "flarm2024packet.hpp"
// // #include "ace/ognconv.hpp"
// //#include "mockutils.h"

// class Test : public etl::message_router<Test, GATAS::AircraftPositionMsg>
// {

// public:
//     GATAS::AircraftPositionInfo position;
//     etl::imessage_bus *bus;
//     Test(etl::imessage_bus *bus_) : bus(bus_)
//     {
//         bus->subscribe(*this);
//     }
//     ~Test()
//     {
//         bus->unsubscribe(*this);
//     }

//     void on_receive(const GATAS::AircraftPositionMsg &msg)
//     {
//         printf("AircraftPosition Received");
//         position = msg.position;
//     }
//     void on_receive_unknown(const etl::imessage &msg)
//     {
//         (void)msg;
//     }
// };

// GATAS::ThreadSafeBus<50> bus;
// MockConfig mockConfig{bus};
// Flarm2024 flarm{bus, mockConfig};
// auto protocol = Radio::ProtocolConfig{1, GATAS::Modulation::GFSK, GATAS::DataSource::FLARM, 24 + 2, 1*8, 7, {0x55, 0x99, 0xA5, 0xA9, 0x55, 0x66, 0x65, 0x96}}; // 0 FLARM 0 airtime 6ms

// TEST_CASE("RadioPacket size", "[single-file]")
// {
//     REQUIRE( sizeof(Flarm2024::RadioPacket) == Flarm2024::RadioPacket::totalLengthWCRC + 2 ); // this includes checksum + padding to ensure byte allinment
// }

// TEST_CASE("addressTypeToFlarm", "[single-file]")
// {
//     REQUIRE(flarm.addressTypeToFlarm(GATAS::AddressType::RANDOM) == 0);
//     REQUIRE(flarm.addressTypeToFlarm(GATAS::AddressType::ICAO) == 1);
//     REQUIRE(flarm.addressTypeToFlarm(GATAS::AddressType::FLARM) == 2);
//     REQUIRE(flarm.addressTypeToFlarm(GATAS::AddressType::FANET) == 0);
//     REQUIRE(flarm.addressTypeToFlarm(GATAS::AddressType::OGN) == 0);
// }

// TEST_CASE("addressTypeFromFlarm", "[single-file]")
// {

//     REQUIRE(flarm.addressTypeFromFlarm(0) == GATAS::AddressType::RANDOM);
//     REQUIRE(flarm.addressTypeFromFlarm(1) == GATAS::AddressType::ICAO);
//     REQUIRE(flarm.addressTypeFromFlarm(2) == GATAS::AddressType::FLARM);

//     for (int i = 3; i < 0xFF; i++)
//     {
//         REQUIRE(flarm.addressTypeFromFlarm(i) == GATAS::AddressType::RANDOM);
//     }
// }



// /* http://en.wikipedia.org/wiki/XXTEA */
// inline void softRfMosheBtea(uint32_t *v, int8_t n, const uint32_t key[4])
// {
//     uint32_t y, z, sum;
//     uint32_t p, rounds, e;

// #define DELTA 0x9e3779b9
// // #define ROUNDS (6 + 52 / n)
// #define ROUNDS 6

//     if (n > 1)
//     {
//         /* Coding Part */
//         rounds = ROUNDS;
//         sum = 0;
//         z = v[n - 1];
//         do
//         {
//             sum += DELTA;
//             e = (sum >> 2) & 3;
//             for (p = 0; p < n - 1; p++)
//             {
//                 y = v[p + 1];
//                 z = v[p] += (((z >> 5 ^ y << 2) + (y >> 3 ^ z << 4)) ^ ((sum ^ y) + (key[(p & 3) ^ e] ^ z)));
//             }
//             y = v[0];
//             z = v[n - 1] += (((z >> 5 ^ y << 2) + (y >> 3 ^ z << 4)) ^ ((sum ^ y) + (key[(p & 3) ^ e] ^ z)));
//         } while (--rounds);
//     }
//     else if (n < -1)
//     {
//         /* Decoding Part */
//         n = -n;
//         rounds = ROUNDS;
//         sum = rounds * DELTA;
//         y = v[0];
//         do
//         {
//             e = (sum >> 2) & 3;
//             for (p = n - 1; p > 0; p--)
//             {
//                 z = v[p - 1];
//                 y = v[p] -= (((z >> 5 ^ y << 2) + (y >> 3 ^ z << 4)) ^ ((sum ^ y) + (key[(p & 3) ^ e] ^ z)));
//             }
//             z = v[n - 1];
//             y = v[0] -= (((z >> 5 ^ y << 2) + (y >> 3 ^ z << 4)) ^ ((sum ^ y) + (key[(p & 3) ^ e] ^ z)));
//             sum -= DELTA;
//         } while (--rounds);
//     }
// }

// // first stage of decrypting
// inline void softRfMosheBtea2(uint32_t *data, bool encode)
// {
//     // for new protocol, ths btea() stage uses fixed keys
//     static const uint32_t keys[4] = {0xa5f9b21c, 0xab3f9d12, 0xc6f34e34, 0xd72fa378};
//     softRfMosheBtea(data + 2, (encode ? 4 : -4), keys);
// }

// inline void softRfMosheScramble(uint32_t *data, uint32_t timestamp)
// {

//     uint32_t wkeys[4];
//     wkeys[0] = data[0];
//     wkeys[1] = data[1];
//     wkeys[2] = (timestamp >> 4);
//     wkeys[3] = 0x956f6c77; // the scramble KEY

//     int z, y, x, sum, p, q;
//     // int n = 16;                        // do by bytes instead of longwords
//     uint8_t *bkeys = (uint8_t *)wkeys;

//     z = bkeys[15];
//     sum = 0;
//     q = 2; // only 2 iterations
//     do
//     {
//         sum += DELTA;
//         y = bkeys[0];
//         for (p = 0; p < 15; p++)
//         {
//             x = y;
//             y = bkeys[p + 1];
//             x += ((((z >> 5) ^ (y << 2)) + ((y >> 3) ^ (z << 4))) ^ (sum ^ y));
//             bkeys[p] = (uint8_t)x;
//             z = x & 0xff;
//         }
//         x = y;
//         y = bkeys[0];
//         x += ((((z >> 5) ^ (y << 2)) + ((y >> 3) ^ (z << 4))) ^ (sum ^ y));
//         bkeys[15] = (uint8_t)x;
//         z = x & 0xff;
//     } while (--q > 0);

//     // now XOR results with last 4 words of the packet
//     data[2] ^= wkeys[0];
//     data[3] ^= wkeys[1];
//     data[4] ^= wkeys[2];
//     data[5] ^= wkeys[3];
// }

// TEST_CASE("bteaAndScramble compatibility SoftRF Moshe functions", "[single-file]")
// {
//     const uint32_t org[] = {0x12345678, 0x12345678, 0x12345678, 0x12345678, 0x12345678, 0x12345678, 0xabcd5678};
//     uint32_t work[] = {0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000};

//     memcpy(work, org, sizeof(org));

//     flarm.scramble(work, 1720119032);
//     flarm.bteaEncode(work + 2);

//     softRfMosheBtea2(work, false);
//     softRfMosheScramble(work, 1720119032);
//     // printf("%4d %4d %8d %2d \n", i, memcmp(work, org, 26), t >> 4, (t & 0x0f));
//     REQUIRE(memcmp(work, org, sizeof(org)) == 0);

//     // print_buffer_hex_uint32(work, 7);
//     REQUIRE((work[6] & 0xFFFF0000) == 0xabcd0000);
// }
