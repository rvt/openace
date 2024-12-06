// (c) 2003, Pawel Jalocha, Pawel.Jalocha@cern.ch
//
// Added with permission of Pawel Jalocha (and reformatted for readability ...sorry...)

#pragma once

#include <stdio.h>

#include <string.h>
#include <stdint.h>
#include "pico/stdlib.h"

#include "ace/ognconv.hpp"

// #include "intmath.h"

#include "ace/bitcount.hpp"
#include "ace/encryption.hpp"
#include "etl/absolute.h"
// #include "nmea.h"
// #include "mavlink.h"


// the packet description here is how it look on the little-endian CPU before sending it to the RF chip
// nRF905, CC1101, SPIRIT1, RFM69 chips actually reverse the bit order within every byte
// thus on the air the bits appear MSbit first for every byte transmitted

class OGN1_Packet          // Packet structure for the OGN tracker
{
public:

    static const int Words =  5;
    static const int Bytes = 20;

    union
    {
        uint32_t HeaderWord;    //    ECRR PMTT AAAA AAAA AAAA AAAA AAAA AAAA
        // E=Emergency, C=enCrypt/Custom, RR=Relay count, P=Parity, M=isMeteo/Other, TT=address Type, AA..=Address:24-bit
        // When enCrypt/Custom is set the data (position or whatever) can only be decoded by the owner
        // This option is indented to pass any type of custom data not foreseen otheriwse
        struct
        {
            unsigned int Address    :24; // aircraft address
            unsigned int AddrType   : 2; // address type: 0 = random, 1 = ICAO, 2 = FLARM, 3 = OGN
            unsigned int NonPos     : 1; // 0 = position packet, 1 = other information like status
            unsigned int Parity     : 1; // parity takes into account bits 0..27 thus only the 28 lowest bits
            unsigned int Relay      : 2; // 0 = direct packet, 1 = relayed once, 2 = relayed twice, ...
            unsigned int Encrypted  : 1; // packet is encrypted
            unsigned int Emergency  : 1; // aircraft in emergency (not used for now)
        } Header ;

    } ;

    union
    {
        uint32_t Data[4];       // 0: QQTT TTTT LLLL LLLL LLLL LLLL LLLL LLLL  QQ=fix Quality:2, TTTTTT=time:6, LL..=Latitude:20
        // 1: MBDD DDDD LLLL LLLL LLLL LLLL LLLL LLLL  F=fixMode:1 B=isBaro:1, DDDDDD=DOP:6, LL..=Longitude:20
        // 2: RRRR RRRR SSSS SSSS SSAA AAAA AAAA AAAA  RR..=turn Rate:8, SS..=Speed:10, AA..=Alt:14
        // 3: BBBB BBBB YYYY PCCC CCCC CCDD DDDD DDDD  BB..=Baro altitude:8, YYYY=AcftType:4, P=Stealth:1, CC..=Climb:9, DD..=Heading:10

        // meteo/telemetry types: Meteo conditions, Thermal wind/climb, Device status, Precise time,

        // meteo report: Humidity, Barometric pressure, Temperature, wind Speed/Direction
        // 2: HHHH HHHH SSSS SSSS SSAA AAAA AAAA AAAA
        // 3: TTTT TTTT YYYY BBBB BBBB BBDD DDDD DDDD  YYYY = report tYpe (meteo, thermal, water level, other telemetry)

        // Device status: Time, baro pressure+temperature, GPS altitude, supply voltage, TX power, RF noise, software version, software features, hardware features,
        // 0: UUUU UUUU UUUU UUUU UUUU UUUU UUUU UUUU  UU..=Unix time
        // 1: SSSS SSSS SSSS SSSS TTTT TTTT HHHH HHHH  SS..=slot time, TT..=temperature, HH..=humidity
        // 2: BBBB BBBB BBBB BBBB BBAA AAAA AAAA AAAA  Baro pressure[0.5Pa], GPS altitude
        // 3: VVVV VVVV YYYY HHHH HHHH XXXX VVVV VVVV  VV..=firmware version, YYYY = report type, TT..=Temperatature, XX..=TxPower, VV..=battery voltage

        // Pilot status:
        // 0: NNNN NNNN NNNN NNNN NNNN NNNN NNNN NNNN  Name: 9 char x 7bit or 10 x 6bit or Huffman encoding ?
        // 1: NNNN NNNN NNNN NNNN NNNN NNNN NNNN NNNN
        struct
        {
            signed int    Latitude:24; //                  // QQTT TTTT LLLL LLLL LLLL LLLL LLLL LLLL  QQ=fix Quality:2, TTTTTT=time:6, LL..=Latitude:24
            unsigned int        Time: 6; // [sec]            // time, just second thus ambiguity every every minute
            unsigned int  FixQuality: 2; //                  // 0 = none, 1 = GPS, 2 = Differential GPS (can be WAAS)
            signed int   Longitude:24; //                  // MBDD DDDD LLLL LLLL LLLL LLLL LLLL LLLL  F=fixMode:1 B=isBaro:1, DDDDDD=DOP:6, LL..=Longitude:24
            unsigned int         DOP: 6; //                  // GPS Dilution of Precision
            unsigned int     BaroMSB: 1; //                  // negated bit #8 of the altitude difference between baro and GPS
            unsigned int     FixMode: 1; //                  // 0 = 2-D, 1 = 3-D
            unsigned int    Altitude:14; // [m] VR           // RRRR RRRR SSSS SSSS SSAA AAAA AAAA AAAA  RR..=turn Rate:8, SS..=Speed:10, AA..=Alt:14
            unsigned int       Speed:10; // [0.1m/s] VR
            unsigned int    TurnRate: 8; // [0.1deg/s] VR
            unsigned int     Heading:10; // [360/1024deg]    // BBBB BBBB YYYY PCCC CCCC CCDD DDDD DDDD  BB..=Baro altitude:8, YYYY=AcftType:4, P=Stealth:1, CC..=Climb:9, DD..=Heading:10
            unsigned int   ClimbRate: 9; // [0.1m/s] VR      // rate of climb/decent from GPS or from baro sensor
            unsigned int     Stealth: 1; //                  // not really used till now
            unsigned int    AcftType: 4; // [0..15]          // type of aircraft: 1 = glider, 2 = towplane, 3 = helicopter, ...
            unsigned int BaroAltDiff: 8; // [m]              // lower 8 bits of the altitude difference between baro and GPS
        } Position;

        struct
        {
            unsigned int Pulse     : 8; // [bpm]               // pilot: heart pulse rate
            unsigned int Oxygen    : 7; // [%]                 // pilot: oxygen level in the blood
            unsigned int SatSNR    : 5; // [dB]                // average SNR of GPS signals
            // unsigned int FEScurr   : 5; // [A]                 // FES current
            unsigned int RxRate    : 4; // [/min]              // log2 of received packet rate
            unsigned int Time      : 6; // [sec]               // same as in the position packet
            unsigned int FixQuality: 2;
            unsigned int AudioNoise: 8; // [dB]                //
            unsigned int RadioNoise: 8; // [dBm]               // noise seen by the RF chip
            unsigned int Temperature:8; // [0.1degC] VR        // temperature by the baro or RF chip
            unsigned int Humidity  : 8; // [0.1%] VR           // humidity
            unsigned int Altitude  :14; // [m] VR              // same as in the position packet
            unsigned int Pressure  :14; // [0.08hPa]           // barometric pressure
            unsigned int Satellites: 4; // [ ]
            unsigned int Firmware  : 8; // [ ]                 // firmware version
            unsigned int Hardware  : 8; // [ ]                 // hardware version
            unsigned int TxPower   : 4; // [dBm]               // RF trancmitter power (offset = 4)
            unsigned int ReportType: 4; // [0]                 // 0 for the status report
            unsigned int Voltage   : 8; // [1/64V] VR          // supply/battery voltage
        } Status;

        union
        {
            uint8_t Byte[16];
            struct
            {
                uint8_t Data[14];      // [16x7bit]packed string of 16-char: 7bit/char
                uint8_t DataChars:  4; // [int] number of characters in the packed string
                uint8_t ReportType: 4; // [1]                 // 1 for the Info packets
                uint8_t Check;         // CRC check
            } ;
        } Info;

        struct
        {
            signed int    Latitude:24; //                  // Latitude of the measurement
            unsigned int        Time: 6; // [sec]            // time, just second thus ambiguity every every minute
            unsigned int            : 2; //                  // spare
            signed int   Longitude:24; //                  // Longitude of the measurement
            unsigned int            : 6; //                  // spare
            unsigned int     BaroMSB: 1; //                  // negated bit #8 of the altitude difference between baro and GPS
            unsigned int            : 1; //                  // spare
            unsigned int    Altitude:14; // [m] VR           // Altitude of the measurement
            unsigned int       Speed:10; // [0.1m/s] VR      // Horizontal wind speed
            unsigned int            : 8; //                  // spare
            unsigned int     Heading:10; //                  // Wind direction
            unsigned int   ClimbRate: 9; // [0.1m/s] VR      // Vertical wind speed
            unsigned int            : 1; //                  // spare
            unsigned int  ReportType: 4; //                  // 2 for wind/thermal report
            unsigned int BaroAltDiff: 8; // [m]              // lower 8 bits of the altitude difference between baro and GPS
        } Wind;

        struct
        {
            uint8_t Data[14];                           // up to 14 bytes od specific data
            unsigned int DataLen   : 4;                      // 0..14 number of bytes in the message
            unsigned int ReportType: 4;                      // 15 for the manufacturer specific mesage
            unsigned int ManufID   : 8;                      // Manufacturer identification: 0 for Cube-Board
        } ManufMsg;                                        // manufacturer-specific message

    } ;

    OGN1_Packet() : HeaderWord(0) {
        memset(Data, 0, sizeof(Data));
    }

    // Added to ensure packet length is of correct size
    uint32_t FEC[2];

    uint8_t  *Byte(void) const
    {
        return (uint8_t  *)&HeaderWord;    // packet as bytes
    }
    uint32_t *Word(void) const
    {
        return (uint32_t *)&HeaderWord;    // packet as words
    }

    // void recvBytes(const uint8_t *SrcPacket) { memcpy(Byte(), SrcPacket, Bytes); } // load data bytes e.g. from a demodulator

    static const uint8_t InfoParmNum = 15; // [int]  number of info-parameters and their names
    static const char *InfoParmName(uint8_t Idx)
    {
        static const char *Name[InfoParmNum] =
        {
            "Pilot", "Manuf", "Model", "Type", "SN", "Reg", "ID", "Class",
            "Task", "Base", "ICE", "PilotID", "Hard", "Soft", "Crew"
        } ;
        return Idx<InfoParmNum ? Name[Idx]:0;
    }

    // calculate distance vector [LatDist, LonDist] from a given reference [RefLat, Reflon]
    int calcDistanceVector(int32_t &LatDist, int32_t &LonDist, int32_t RefLat, int32_t RefLon, uint16_t LatCos=3000, int32_t MaxDist=0x7FFF)
    {
        LatDist = DecodeLatitude()-RefLat;
        if (etl::absolute(LatDist)>1080000) return -1; // to prevent overflow, corresponds to about 200km
        LatDist = (LatDist*1517+0x1000)>>13;              // convert from 1/600000deg to meters (40000000m = 360deg) => x 5/27 = 1517/(1<<13)
        if (etl::absolute(LatDist)>MaxDist) return -1;
        LonDist = DecodeLongitude()-RefLon;
        if (etl::absolute(LatDist)>1080000) return -1;
        LonDist = (LonDist*1517+0x1000)>>13;
        if (etl::absolute(LonDist)>(4*MaxDist)) return -1;
        LonDist = (LonDist*LatCos+0x800)>>12;
        if (etl::absolute(LonDist)>MaxDist) return -1;
        return 1;
    }

    // sets position [Lat, Lon] according to given distance vector [LatDist, LonDist] from a reference point [RefLat, RefLon]
    void setDistanceVector(int32_t LatDist, int32_t LonDist, int32_t RefLat, int32_t RefLon, uint16_t LatCos=3000)
    {
        EncodeLatitude(RefLat+(LatDist*27)/5);
        LonDist = (LonDist<<12)/LatCos;                                  // LonDist/=cosine(Latitude)
        EncodeLongitude(RefLon+(LonDist*27)/5);
    }

    // Centripetal acceleration
    static int16_t calcCPaccel(int16_t Speed, int16_t TurnRate)
    {
        return ((int32_t)TurnRate*Speed*229+0x10000)>>17;    // [0.1m/s^2]
    }
    int16_t calcCPaccel(void)
    {
        return calcCPaccel(DecodeSpeed(), DecodeTurnRate());
    }

    // Turn radius
    static int16_t calcTurnRadius(int16_t Speed, int16_t TurnRate, int16_t MaxRadius=0x7FFF) // [m] ([0.1m/s], [], [m])
    {
        if (TurnRate==0) return 0;
        int32_t Radius = 14675*Speed;
        Radius /= TurnRate;
        Radius = (Radius+128)>>8;
        if (etl::absolute(Radius)>MaxRadius) return 0;
        return Radius;
    }
    int16_t calcTurnRadius(int16_t MaxRadius=0x7FFF)
    {
        return calcTurnRadius(DecodeSpeed(), DecodeTurnRate(), MaxRadius);
    }

    // OGN1_Packet() { Clear(); }
    void Clear(void)
    {
        HeaderWord=0;
        Data[0]=0;
        Data[1]=0;
        Data[2]=0;
        Data[3]=0;
    }

    uint32_t getAddressAndType(void) const
    {
        return HeaderWord&0x03FFFFFF;    // Address with address-type: 26-bit
    }
    void     setAddressAndType(uint32_t AddrAndType)
    {
        HeaderWord = (HeaderWord&0xFC000000) | (AddrAndType&0x03FFFFFF);
    }

    bool goodAddrParity(void) const
    {
        return ((Count1s(HeaderWord&0x0FFFFFFF)&1)==0);    // Address parity should be EVEN
    }
    void calcAddrParity(void)
    {
        if (!goodAddrParity()) HeaderWord ^= 0x08000000;    // if not correct parity, flip the parity bit
    }

    void EncodeLatitude(int32_t Latitude)                                // encode Latitude: units are 0.0001/60 degrees
    {
        Position.Latitude = Latitude>>3;
    }

    int32_t DecodeLatitude(void) const
    {
        int32_t Latitude = Position.Latitude;
        // if (Latitude&0x00800000) Latitude|=0xFF000000;
        Latitude = (Latitude<<3)+4;
        return Latitude;
    }

    void EncodeLongitude(int32_t Longitude)                             // encode Longitude: units are 0.0001/60 degrees
    {
        Position.Longitude = Longitude>>=4;
    }

    int32_t DecodeLongitude(void) const
    {
        int32_t Longitude = Position.Longitude;
        Longitude = (Longitude<<4)+8;
        return Longitude;
    }

    bool hasBaro(void) const
    {
        return Position.BaroMSB || Position.BaroAltDiff;
    }
    void clrBaro(void)
    {
        Position.BaroMSB=0;
        Position.BaroAltDiff=0;
    }
    int16_t getBaroAltDiff(void) const
    {
        int16_t AltDiff=Position.BaroAltDiff;
        if (Position.BaroMSB==0) AltDiff|=0xFF00;
        return AltDiff;
    }
    void setBaroAltDiff(int32_t AltDiff)
    {
        // if (AltDiff<(-255)) AltDiff=(-255); else if (AltDiff>255) AltDiff=255;
        if (AltDiff<(-255) || AltDiff>255)
        {
            clrBaro();
            return;
        }
        Position.BaroMSB = (AltDiff&0xFF00)==0;
        Position.BaroAltDiff=AltDiff&0xFF;
    }
    void EncodeStdAltitude(int32_t StdAlt)
    {
        setBaroAltDiff((StdAlt-DecodeAltitude()));
    }
    int32_t DecodeStdAltitude(void) const
    {
        return (DecodeAltitude()+getBaroAltDiff());
    }

    void EncodeAltitude(int32_t Altitude)                               // encode altitude in meters
    {
        if (Altitude<0)      Altitude=0;
        Position.Altitude = UnsVRencode<uint16_t, 12>((uint16_t)Altitude);
    }
//     Position.Altitude = EncodeUR2V12((uint16_t)Altitude); }

    int32_t DecodeAltitude(void) const                                   // return Altitude in meters
    {
        return UnsVRdecode<uint16_t, 12>(Position.Altitude);
    }
//   { return DecodeUR2V12(Position.Altitude); }

    void EncodeDOP(uint8_t DOP)
    {
        Position.DOP = UnsVRencode<uint8_t, 4>(DOP);
    }
//   { Position.DOP = EncodeUR2V4(DOP); }

    uint8_t DecodeDOP(void) const
    {
        return UnsVRdecode<uint8_t, 4>(Position.DOP);
    }
//   { return DecodeUR2V4(Position.DOP); }

    void EncodeSpeed(int16_t Speed)                                       // speed in 0.2 knots (or 0.1m/s)
    {
        if (Speed<0) Speed=0;
        else Speed = UnsVRencode<uint16_t, 8>(Speed); // EncodeUR2V8(Speed);
        Position.Speed = Speed;
    }

    int16_t DecodeSpeed(void) const           // return speed in 0.2 knots or 0.1m/s units
    {
        return UnsVRdecode<uint16_t, 8>(Position.Speed);
    }
//   { return DecodeUR2V8(Position.Speed); }   // => max. speed: 3832*0.2 = 766 knots

    int16_t DecodeHeading(void) const         // return Heading in 0.1 degree units 0..359.9 deg
    {
        int32_t Heading = Position.Heading;
        return (Heading*3600+512)>>10;
    }

    void EncodeHeading(int16_t Heading)
    {
        Position.Heading = (((int32_t)Heading<<10)+180)/3600;
    }

    void setHeadingAngle(uint16_t HeadingAngle)
    {
        Position.Heading = (((HeadingAngle+32)>>6));
    }

    uint16_t getHeadingAngle(void) const
    {
        return (uint16_t)Position.Heading<<6;
    }

    void clrTurnRate(void)
    {
        Position.TurnRate=0x80;    // mark turn-rate as absent
    }
    bool hasTurnRate(void) const
    {
        return Position.TurnRate!=0x80;
    }

    void EncodeTurnRate(int16_t Turn)                                      // [0.1 deg/sec]
    {
        Position.TurnRate = EncodeSR2V5(Turn);
    }

    int16_t DecodeTurnRate(void) const
    {
        return DecodeSR2V5(Position.TurnRate);
    }

    void clrClimbRate(void)
    {
        Position.ClimbRate=0x100;    // mark climb rate as absent
    }
    bool hasClimbRate(void) const
    {
        return Position.ClimbRate!=0x100;
    }

    void EncodeClimbRate(int16_t Climb)
    {
        Position.ClimbRate = EncodeSR2V6(Climb);
    }

    int16_t DecodeClimbRate(void) const
    {
        return DecodeSR2V6(Position.ClimbRate);
    }

// --------------------------------------------------------------------------------------------------------------
// Status fields

    void clrTemperature(void)
    {
        Status.Temperature=0x80;
    }
    bool hasTemperature(void)       const
    {
        return Status.Temperature!=0x80;
    }
    void EncodeTemperature(int16_t Temp)
    {
        Status.Temperature=EncodeSR2V5(Temp-200);    // [0.1degC]
    }
    int16_t DecodeTemperature(void) const
    {
        return 200+DecodeSR2V5(Status.Temperature);
    }

    void EncodeVoltage(uint16_t Voltage)
    {
        Status.Voltage=EncodeUR2V6(Voltage);    // [1/64V]
    }
    uint16_t DecodeVoltage(void) const
    {
        return DecodeUR2V6(Status.Voltage);
    }

    void clrHumidity(void)
    {
        Status.Humidity=0x80;
    }
    bool hasHumidity(void)        const
    {
        return Status.Humidity!=0x80;
    }
    void EncodeHumidity(uint16_t Hum)
    {
        Status.Humidity=EncodeSR2V5((int16_t)(Hum-520));    // [0.1%]
    }
    uint16_t DecodeHumidity(void) const
    {
        return 520+DecodeSR2V5(Status.Humidity);
    }

// --------------------------------------------------------------------------------------------------------------
// Info fields: pack and unpack 7-bit char into the Info packets

    void setInfoChar(uint8_t Char, uint8_t Idx)                           // put 7-bit Char onto give position
    {
        if (Idx>=16) return;                                                 // Idx = 0..15
        Char&=0x7F;
        uint8_t BitIdx = Idx*7;                                             // [bits] bit index to the target field
        Idx = BitIdx>>3;                                            // [bytes] index of the first byte to change
        uint8_t Ofs = BitIdx&0x07;
        if (Ofs==0)
        {
            Info.Data[Idx] = (Info.Data[Idx]&0x80) |  Char     ;
            return;
        }
        if (Ofs==1)
        {
            Info.Data[Idx] = (Info.Data[Idx]&0x01) | (Char<<1) ;
            return;
        }
        uint8_t Len1 = 8-Ofs;
        uint8_t Len2 = Ofs-1;
        uint8_t Msk1 = 0xFF;
        Msk1<<=Ofs;
        uint8_t Msk2 = 0x01;
        Msk2 = (Msk2<<Len2)-1;
        Info.Data[Idx  ] = (Info.Data[Idx  ]&(~Msk1)) | (Char<<Ofs);
        Info.Data[Idx+1] = (Info.Data[Idx+1]&(~Msk2)) | (Char>>Len1);
    }

    uint8_t getInfoChar(uint8_t Idx) const                                // get 7-bit Char from given position
    {
        if (Idx>=16) return 0;                                               // Idx = 0..15
        uint8_t BitIdx = Idx*7;                                             // [bits] bit index to the target field
        Idx = BitIdx>>3;                                            // [bytes] index of the first byte to change
        uint8_t Ofs = BitIdx&0x07;
        if (Ofs==0) return Info.Data[Idx]&0x7F;
        if (Ofs==1) return Info.Data[Idx]>>1;
        uint8_t Len = 8-Ofs;
        return (Info.Data[Idx]>>Ofs) | ((Info.Data[Idx+1]<<Len)&0x7F);
    }

    void clrInfo(void)                                    // clear the info packet
    {
        Info.DataChars=0;                                   // clear number of characters
        Info.ReportType=1;
    }                                // just in case: set the report-type

    uint8_t addInfo(const char *Value, uint8_t InfoType)  // add an info field
    {
        uint8_t Idx=Info.DataChars;                         // number of characters already in the info packet
        if (Idx) Idx++;                                      // if at least one already, then skip over the terminator
        if (Idx>=15) return 0;
        uint8_t Len=0;
        for ( ; ; )
        {
            uint8_t Char = Value[Len];
            if (Char==0) break;
            if (Idx>=15) return 0;
            setInfoChar(Char, Idx++);
            Len++;
        }
        setInfoChar(InfoType, Idx);                         // terminating character
        Info.DataChars=Idx;                                 // update number of characters
        return Len+1;
    }                                     // return number of added Value characters

    uint8_t readInfo(char *Value, uint8_t &InfoType, uint8_t ValueIdx=0) const
    {
        uint8_t Len=0;                                       // count characters in the info-string
        uint8_t Chars = Info.DataChars;                      // total number of characters in the record
        char Char=0;
        for ( ; ; )                                           // loop over characters
        {
            if ((ValueIdx+Len)>Chars) return 0;                 // return failure if overrun the data
            Char = getInfoChar(ValueIdx+Len);                  // get the character
            if (Char<0x20) break;                               // if less than 0x20 (space) then this is the terminator
            Value[Len++]=Char;
        }
        Value[Len]=0;                                        // null-terminate the infor string
        InfoType=Char;                                       // get the info-type: Pilot, Type, etc.
        return Len+1;
    }                                      // return number of character taken thus info length + terminator

    uint8_t InfoCheck(void) const
    {
        uint8_t Check=0;
        for ( uint8_t Idx=0; Idx<15; Idx++)
        {
            Check ^= Info.Byte[Idx];
        }
        // printf("Check = %02X\n", Check);
        return Check;
    }

    void setInfoCheck(void)
    {
        Info.Check = InfoCheck();
        // printf("Check = %02X\n", Info.Check);
    }

    uint8_t goodInfoCheck(void) const
    {
        return Info.Check == InfoCheck();
    }

// --------------------------------------------------------------------------------------------------------------

    void Encrypt (const uint32_t Key[4])
    {
        xxteaEncrypt(Data, 4, Key, 8);    // encrypt with given Key
    }
    void Decrypt (const uint32_t Key[4])
    {
        xxteaEncrypt(Data, 4, Key, 8);    // decrypt with given Key
    }

    void Whiten  (void)
    {
        TEA_Encrypt_Key0(Data, 8);    // whiten the position
        TEA_Encrypt_Key0(Data+2, 8);
    }
    void Dewhiten(void)
    {
        TEA_Decrypt_Key0(Data, 8);    // de-whiten the position
        TEA_Decrypt_Key0(Data+2, 8);
    }



} ;
