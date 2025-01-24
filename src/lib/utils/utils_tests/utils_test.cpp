
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "math.h"

#define private public

#include <stdio.h>

#include "EMA.hpp"
#include "encryption.hpp"
#include "utils.hpp"
#include "mockutils.h"
#include "manchester.hpp"

constexpr float MS_TO_FTPMIN     = 196.850394f;         // meter/sec to feet/min
constexpr float DEG_TO_RADS      = M_PI / 180.f;        // degrees to radians

TEST_CASE( "swapBytes16", "[single-file]" )
{
    REQUIRE( (swapBytes16(0x1234) == 0x3412 ) );
    REQUIRE( (swapBytes16(0xABCD) == 0xCDAB ) );
}

TEST_CASE( "manchesterEncodeTest", "[single-file]" )
{
    uint8_t decode[] = {0x72, 0x4B};
    uint8_t encoded[] = {0, 0, 0, 0};
    manchesterEncode(encoded, decode, 2);
    printf("Manchester:");
    print_buffer_hex(encoded, 4);
    printf("\n");
}



// TEST_CASE( "AMA", "[single-file]" )
// {
//     // Tuned such that within 5 seconds (5GPS positions per second) the resulting values is within 3%
//     EMAFloat filter{OPENACE_EMAFLOAT_K_FACTOR_1S};
//     printf("1 %f\n", filter(100));
//     printf("2 %f\n", filter(100));
//     printf("3 %f\n", filter(100));
//     printf("4 %f\n", filter(100));
//     REQUIRE( (filter(100) > 96.9 ) );
// //    REQUIRE( (filter() > 96.9 ) );
//     printf("1 %f\n", filter(0));
//     printf("2 %f\n", filter(0));
//     printf("3 %f\n", filter(0));
//     printf("4 %f\n", filter(0));
//     REQUIRE( (filter(0) < 3 ) );
// //    REQUIRE( (filter() < 3 ) );
//     printf("1 %f\n", filter(-100));
//     printf("2 %f\n", filter(-100));
//     printf("3 %f\n", filter(-100));
//     printf("4 %f\n", filter(-100));
//     REQUIRE( (filter(0) < 97 ) );
// //    REQUIRE( (filter() < 97 ) );
// }


// TEST_CASE( "RatePerSecond", "[single-file]" )
// {
//     printf("-------\n");
//     RatePerSecond<5> filter(OPENACE_EMAFLOAT_K_FACTOR_1S);
//     for (int i = 0; i < 15; i++)
//     {
//         filter(500);
//     }
//     int meterPerSec = 1;
//     float result=0;
//     // 5m/s == 984feet/min
//     printf(" %f %f %d\n", filter(500+meterPerSec), filter.perSecond() * MS_TO_FTPMIN, meterPerSec); meterPerSec++;
//     printf(" %f %f %d\n", filter(500+meterPerSec), filter.perSecond() * MS_TO_FTPMIN, meterPerSec); meterPerSec++;
//     printf(" %f %f %d\n", filter(500+meterPerSec), filter.perSecond() * MS_TO_FTPMIN, meterPerSec); meterPerSec++;
//     printf(" %f %f %d\n", filter(500+meterPerSec), filter.perSecond() * MS_TO_FTPMIN, meterPerSec); meterPerSec++;
//     printf(" %f %f %d\n", result = filter(500+meterPerSec), filter.perSecond() * MS_TO_FTPMIN, meterPerSec); meterPerSec++;

//     // Near the 984 min
//     REQUIRE( filter.perSecond()*MS_TO_FTPMIN == Catch::Approx(960).margin(2) );
//     REQUIRE( (result*MS_TO_FTPMIN) == Catch::Approx(192).margin(2) );

//     printf(" %f %f %d\n", filter(500+meterPerSec), filter.perSecond() * MS_TO_FTPMIN, meterPerSec); meterPerSec++;
//     printf(" %f %f %d\n", result = filter(500+meterPerSec), filter.perSecond() * MS_TO_FTPMIN, meterPerSec); meterPerSec++;
//     // Near the 978 min
//     REQUIRE( filter.perSecond()*MS_TO_FTPMIN == Catch::Approx(978).margin(2) );
//     REQUIRE( (result*MS_TO_FTPMIN) == Catch::Approx(195).margin(2) );

//     printf(" %f %f %d\n", filter(500+meterPerSec), filter.perSecond() * MS_TO_FTPMIN, meterPerSec);
//     printf(" %f %f %d\n", filter(500+meterPerSec), filter.perSecond() * MS_TO_FTPMIN, meterPerSec);
//     printf(" %f %f %d\n", filter(500+meterPerSec), filter.perSecond() * MS_TO_FTPMIN, meterPerSec);
//     printf(" %f %f %d\n", filter(500+meterPerSec), filter.perSecond() * MS_TO_FTPMIN, meterPerSec);
//     printf(" %f %f %d\n", result = filter(500+meterPerSec), filter.perSecond() * MS_TO_FTPMIN, meterPerSec);

//     // No updates. rate/sec goes down
//     REQUIRE( filter.perSecond()*MS_TO_FTPMIN == Catch::Approx(49).margin(2) );
//     REQUIRE( (result*MS_TO_FTPMIN) == Catch::Approx(10).margin(2) );

// //    fmeterlter.print();
// //    filter.print();
// }


// TEST_CASE( "buffersParity8", "[single-file]" )
// {
//     uint8_t buffer[4] = {0x01, 0x02, 0x04, 0x08};
//     REQUIRE( (buffersParity8(buffer, 4) == 0 ) );
//     buffer[3] = 0x09;
//     REQUIRE( (buffersParity8(buffer, 4) == 1 ) );
//     buffer[0] = 0x03;
//     REQUIRE( (buffersParity8(buffer, 4) == 0 ) );
// }

// TEST_CASE( "xxteaEncrypt and XXTEA_Decrypt_Key0", "[single-file]" )
// {
//     uint32_t key[4] = {0, 0, 0, 0};
//     uint32_t buffer1[] = {0x01, 0x02, 0x04, 0x08, 0x16, 0x32, 0x64, 0xF0};
//     printf("xxteaEncrypt:");
//     print_buffer_hex_uint32(buffer1, 8);
//     printf("\n");
//     xxteaEncrypt(buffer1, 8, key, 4);

//     printf("Encrypted: ");
//     print_buffer_hex_uint32(buffer1, 8);
//     printf("\n");

//     printf("Decrypted: ");
//     XXTEA_Decrypt_Key0(buffer1, 8, 4);
//     print_buffer_hex_uint32(buffer1, 8);
//     printf("\n");

//     REQUIRE( (buffer1[7] == 0xF0 ) );
// }

