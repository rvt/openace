#pragma once

#include <stdint.h>
#include "ace/ognconv.hpp"
#include "ace/bitcount.hpp"
#include "ace/encryption.hpp"

class ADSL_Packet
{

public:

    typedef enum
    {
        RandomPrivact = 0,
        Reserved1 = 1,
        Reserved2 = 2,
        Reserved3 = 3,
        Reserved4 = 4,
        ICAO = 5,
        FlarmAndOEM = 6,
        OGN = 7,
        FanetAndOEM = 8,
        ManufacvturersPage0 = 9 // ..54
    } AddressMappingTableEntry;

    typedef enum
    {
        FS_Undefined = 0,
        FS_OnGround = 1,
        FS_Airborne = 2,
        FS_Reserved = 3
    } FlightState;

    typedef enum
    {
        AC_NoEmitterCategory = 0,
        AC_LightFixedWing = 1,
        AC_SmallToHeavyFixedWing = 2,
        AC_Rotorcraft = 3,
        AC_GliderSailplane = 4,
        AC_LighterThanAir = 5,
        AC_Ultralight = 6,
        AC_HangGliderParaglider = 7,
        AC_ParachutistSkydiverWingsuit = 8,
        AC_EVTOL_UAM = 9,
        AC_Gyrocopter = 10,
        AC_UASOpenCategory = 11,
        AC_UASSpecificCategory = 12,
        AC_UASCertifiedCategory = 13,
        AC_AircraftCategoryReserved = 14 // .. 31
    } AircraftCategory;

    typedef enum
    {
        ES_EmergencyStatusUndefined = 0,
        ES_NoEmergency = 1,
        ES_GeneralEmergency = 2,
        ES_LifeguardMedicalEmergency = 3,
        ES_NoCommunications = 4,
        ES_UnlawfulInterference = 5,
        ES_DownedAircraft = 6,
        ES_EmergencyStatusReserved = 7
    } EmergencyStatus;

    typedef enum
    {
        SI_IntegrityLess1e7 = 3,
        SI_IntegrityLess1e5 = 2,
        SI_IntegrityLess1e3 = 1,
        SI_IntegrityMore1e3 = 0,
        SI_Undefined = 0
    } SourceIntegrity;

    typedef enum
    {
        DA_None = 0,
        DA_LevelD = 1,
        DA_LevelC = 2,
        DA_LevelB = 3
    } DesignAsurance;

    typedef enum
    {
        NI_Undefined = 0,
        NI_GreaterThanEqual20Nm = 1,
        NI_LessThan20Nm = 2,
        NI_LessThan8Nm = 3,
        NI_LessThan4Nm = 4,
        NI_LessThan2Nm = 5,
        NI_LessThan1Nm = 6,
        NI_LessThan0_6Nm = 7,
        NI_LessThan0_2Nm = 8,
        NI_LessThan0_1Nm = 9,
        NI_LessThan75m = 10,
        NI_LessThan25m = 11,
        NI_LessThan7_5m = 12
    } NavigationIntegrity;

    typedef enum
    {
        HPA_Unknown = 0,
        HPA_LessThan0_5Nm = 1,
        HPA_LessThan0_3Nm = 2,
        HPA_LessThan0_1Nm = 3,
        HPA_LessThan0_05Nm = 4,
        HPA_LessThan30m = 5,
        HPA_LessThan10m = 6,
        HPA_LessThan3m = 7
    } HorizontalPositionAccuracy;

    typedef enum
    {
        VPA_UnknownOrNoFixGreaterEqual150m = 0,
        VPA_LessThan150m = 1,
        VPA_LessThan45m = 2,
        VPA_LessThan15m = 3
    } VerticalPositionAccuracy;

    typedef enum
    {
        VA_UnknownOrNoFixGreaterEqual10mPerSec = 0,
        VA_LessThan10mPerSec = 1,
        VA_LessThan3mPerSec = 2,
        VA_LessThan1mPerSec = 3
    } VelocityAccuracy;


public:
#pragma pack(push, 1)
    static constexpr uint8_t TotalTxBytes=24;
    // 2Byte
    uint16_t ALLIGN;        // two bytes for correct alignment for the Word to allow XXTEA_Encrypt_Key0(..) be allign on word boundery

    // 1Byte
    uint8_t length;         // [bytes] Packet length excluding this byte and SYNC but including sCRC, eg everything after this byte

    // 1Byte
    union
    {
        uint8_t header;
        struct __attribute__ ((packed))
        {
            uint8_t version: 4;
            uint8_t signature: 1;
            uint8_t key: 2;
            uint8_t reserved: 1;
        };
    };

    // 20 Byte
    union
    {
        uint32_t Word[5];       // this part to be scrambled/encrypted, is aligned to 32-bit
        struct __attribute__ ((packed))
        {
            uint8_t payloadIdent:8;
            uint8_t addressMapping:6;
            uint32_t address:24;
            uint8_t reserved1:1;
            uint8_t relay:1;
            uint8_t timeStamp: 6;      // 0.25sec second
            FlightState flightState: 2;
            AircraftCategory aircraftCategory: 5;
            EmergencyStatus emergencyStatus: 3;
            int32_t latitude: 24;
            int32_t longitude: 24;
            uint8_t groundSpeed: 8;
            uint16_t altitudeGeoid: 14;
            int16_t verticalRate: 9;
            uint16_t groundTrack: 9;
            SourceIntegrity sourceIntegrity: 2;
            DesignAsurance designAssurance: 2;
            NavigationIntegrity navigationIntegrity: 4;
            HorizontalPositionAccuracy horizontalPositionAccuracy: 3;
            VerticalPositionAccuracy verticalPositionAccuracy: 2;
            VelocityAccuracy velocityPositionAccuracy: 2;
            uint8_t reserved2: 1;
        };
    };
    // 3 Byte
    uint8_t CRC[3];           // 24-bit (is aligned to 32-bit)
#pragma pack(pop)

public:
    ADSL_Packet() :
        /*SYNC(0x4B72),*/ length(24), version(0), signature(0), key(0), reserved(0) //  Length is from Version
    {
        Word[0] = 0;
        Word[1] = 0;
        Word[2] = 0;
        Word[3] = 0;
        Word[4] = 0;
    }

    static int Correct(uint8_t *PktData, uint8_t *PktErr, const int MaxBadBits=6)
    {
        return Correct_(PktData, PktErr, MaxBadBits);
    }
    inline void Scramble(void)
    {
        XXTEA_Encrypt_Key0(Word, 5, 6);
    }

    inline void Descramble(void)
    {
        XXTEA_Decrypt_Key0(Word, 5, 6);
    }
    void setCRC(void)
    {
        uint32_t Word = calcPI((const uint8_t *)&header, 21);
        CRC[0]=Word>>16;
        CRC[1]=Word>>8;
        CRC[2]=Word;
    }

    int32_t getaltitudeGeoid(void) const                                                                  // [m]
    {
        return UnsVRdecode<int32_t,12>(altitudeGeoid) - 320;
    }
    void setaltitudeGeoid(int32_t altitude)                                                                   // [m]
    {
        if (altitude<-320) altitude = -320;
        altitudeGeoid = UnsVRencode<int32_t,12>(altitude + 320);
    }

    float getLatitude() const
    {
        return FNTtoFloat(latitude << 7);
    }
    void setLatitude(float lat)
    {
        latitude = FloatToFNT(lat) >> 7;
    }

    float getLongitude() const
    {
        return FNTtoFloat(longitude << 8);
    }
    void setLongitude(float lat)
    {
        longitude = FloatToFNT(lat) >> 8;
    }
    float getGroundSpeed() const
    {
        return UnsVRdecode<uint16_t,6>(groundSpeed) * 0.25f;    // [0.25 m/s]
    }
    void setGroundSpeed(float speed)
    {
        groundSpeed = UnsVRencode<uint16_t,6>(speed / 0.25f) ;   // [0.25 m/s]
    }

    static constexpr float TRACKCONV = (360.f / 512.f);
    float getTrack() const
    {
        return groundTrack * TRACKCONV;
    }
    void setTrack(float track)
    {
        groundTrack = track / TRACKCONV;
    }
    uint32_t getAddress() const
    {
        return address;
    }
    float getVerticalRate() const
    {
        if (verticalRate == -256)
        {
            return 0.f;
        }
        return SignVRdecode<int16_t,6>(verticalRate) * 0.125f;
    }
    void setVerticalRate(float vertRate)
    {
        verticalRate = SignVRencode<int16_t,6>(vertRate / 0.125f);
    }
    void setNoVerticalRate()
    {
        verticalRate = -256;
    }
    void setHorAccur(uint8_t Prec)
    {
        if(Prec<= 3)
        {
            horizontalPositionAccuracy=ADSL_Packet::HorizontalPositionAccuracy::HPA_LessThan3m;
            velocityPositionAccuracy = ADSL_Packet::VelocityAccuracy::VA_LessThan1mPerSec;
        }
        else if(Prec<=10)
        {
            horizontalPositionAccuracy=ADSL_Packet::HorizontalPositionAccuracy::HPA_LessThan10m;
            velocityPositionAccuracy = ADSL_Packet::VelocityAccuracy::VA_LessThan3mPerSec;
        }
        else if(Prec<=30)
        {
            horizontalPositionAccuracy=ADSL_Packet::HorizontalPositionAccuracy::HPA_LessThan30m;
            velocityPositionAccuracy =ADSL_Packet::VelocityAccuracy::VA_LessThan10mPerSec;
        }
        else
        {
            horizontalPositionAccuracy=ADSL_Packet::HorizontalPositionAccuracy::HPA_LessThan0_05Nm;
            velocityPositionAccuracy = ADSL_Packet::VelocityAccuracy::VA_UnknownOrNoFixGreaterEqual10mPerSec;
        }
    }

    void setVerAccur(uint8_t Prec)
    {
        if(Prec<=15)
        {
            verticalPositionAccuracy=ADSL_Packet::VerticalPositionAccuracy::VPA_LessThan15m;
        }
        else if(Prec<=45)
        {
            verticalPositionAccuracy=ADSL_Packet::VerticalPositionAccuracy::VPA_LessThan45m;
        }
        else
        {
            verticalPositionAccuracy=ADSL_Packet::VerticalPositionAccuracy::VPA_UnknownOrNoFixGreaterEqual150m;
        }
    }


    inline void * data() {
        return &header;
    }

private:
    /****************************************************************/
    /******************** Packet functions **************************/
    /****************************************************************/
    uint32_t checkCRC(void) const
    {
        return checkPI((const uint8_t *)&header, 24);
    }

    static constexpr float FNTtoFloatConf = 90.0007295677/0x40000000;  // FANET cordic conversion factor (not exactly cordic)
    static float FNTtoFloat(int32_t Coord)                             // convert from FANET cordic units to float degrees
    {
        return FNTtoFloatConf * Coord;
    }

    static int32_t FloatToFNT(float Coord)                             // convert from FANET cordic units to float degrees
    {
        return Coord / FNTtoFloatConf;
    }

    /****************************************************************/
    /******************** Packet functions **************************/
    /****************************************************************/
    static uint32_t calcPI(const uint8_t *Byte, uint8_t Bytes)  // calculate PI for the given packet data excluding the three CRC bytes
    {
        uint32_t CRC = 0;
        for(uint8_t Idx=0; Idx<Bytes; Idx++)
        {
            CRC = PolyPass(CRC, Byte[Idx]);
        }
        CRC=PolyPass(CRC, 0);
        CRC=PolyPass(CRC, 0);
        CRC=PolyPass(CRC, 0);
        return CRC>>8;
    }

    static uint32_t checkPI(const uint8_t *Byte, uint8_t Bytes) // run over data bytes and the three CRC bytes
    {
        uint32_t CRC = 0;
        for(uint8_t Idx=0; Idx<Bytes; Idx++)
        {
            CRC = PolyPass(CRC, Byte[Idx]);
        }
        return CRC>>8;
    }

    static void FlipBit(uint8_t *Byte, int BitIdx)
    {
        int ByteIdx=BitIdx>>3;
        BitIdx&=7;
        BitIdx=7-BitIdx;
        uint8_t Mask=1;
        Mask<<=BitIdx;
        Byte[ByteIdx]^=Mask;
    }

    static uint32_t PolyPass(uint32_t CRC, uint8_t Byte)     // pass a single byte through the CRC polynomial
    {
        const uint32_t Poly = 0xFFFA0480;
        CRC |= Byte;
        for(uint8_t Bit=0; Bit<8; Bit++)
        {
            if(CRC&0x80000000) CRC ^= Poly;
            CRC<<=1;
        }
        return CRC;
    }

    static uint32_t CRCsyndrome(uint8_t Bit)
    {
        const uint16_t PacketBytes = 24;
        const uint16_t PacketBits = PacketBytes*8;
        const uint32_t Syndrome[PacketBits] =
        {
            0x7ABEE1, 0xC2A574, 0x6152BA, 0x30A95D, 0xE7AEAA, 0x73D755, 0xC611AE, 0x6308D7,
            0xCE7E6F, 0x98C533, 0xB3989D, 0xA6364A, 0x531B25, 0xD67796, 0x6B3BCB, 0xCA67E1,
            0x9AC9F4, 0x4D64FA, 0x26B27D, 0xECA33A, 0x76519D, 0xC4D2CA, 0x626965, 0xCECEB6,
            0x67675B, 0xCC49A9, 0x99DED0, 0x4CEF68, 0x2677B4, 0x133BDA, 0x099DED, 0xFB34F2,
            0x7D9A79, 0xC13738, 0x609B9C, 0x304DCE, 0x1826E7, 0xF3E977, 0x860EBF, 0xBCFD5B,
            0xA184A9, 0xAF3850, 0x579C28, 0x2BCE14, 0x15E70A, 0x0AF385, 0xFA83C6, 0x7D41E3,
            0xC15AF5, 0x9F577E, 0x4FABBF, 0xD82FDB, 0x93EDE9, 0xB60CF0, 0x5B0678, 0x2D833C,
            0x16C19E, 0x0B60CF, 0xFA4A63, 0x82DF35, 0xBE959E, 0x5F4ACF, 0xD05F63, 0x97D5B5,
            0xB410DE, 0x5A086F, 0xD2FE33, 0x96851D, 0xB4B88A, 0x5A5C45, 0xD2D426, 0x696A13,
            0xCB4F0D, 0x9A5D82, 0x4D2EC1, 0xD96D64, 0x6CB6B2, 0x365B59, 0xE4D7A8, 0x726BD4,
            0x3935EA, 0x1C9AF5, 0xF1B77E, 0x78DBBF, 0xC397DB, 0x9E31E9, 0xB0E2F0, 0x587178,
            0x2C38BC, 0x161C5E, 0x0B0E2F, 0xFA7D13, 0x82C48D, 0xBE9842, 0x5F4C21, 0xD05C14,
            0x682E0A, 0x341705, 0xE5F186, 0x72F8C3, 0xC68665, 0x9CB936, 0x4E5C9B, 0xD8D449,
            0x939020, 0x49C810, 0x24E408, 0x127204, 0x093902, 0x049C81, 0xFDB444, 0x7EDA22,
            0x3F6D11, 0xE04C8C, 0x702646, 0x381323, 0xE3F395, 0x8E03CE, 0x4701E7, 0xDC7AF7,
            0x91C77F, 0xB719BB, 0xA476D9, 0xADC168, 0x56E0B4, 0x2B705A, 0x15B82D, 0xF52612,
            0x7A9309, 0xC2B380, 0x6159C0, 0x30ACE0, 0x185670, 0x0C2B38, 0x06159C, 0x030ACE,
            0x018567, 0xFF38B7, 0x80665F, 0xBFC92B, 0xA01E91, 0xAFF54C, 0x57FAA6, 0x2BFD53,
            0xEA04AD, 0x8AF852, 0x457C29, 0xDD4410, 0x6EA208, 0x375104, 0x1BA882, 0x0DD441,
            0xF91024, 0x7C8812, 0x3E4409, 0xE0D800, 0x706C00, 0x383600, 0x1C1B00, 0x0E0D80,
            0x0706C0, 0x038360, 0x01C1B0, 0x00E0D8, 0x00706C, 0x003836, 0x001C1B, 0xFFF409,
            0x800000, 0x400000, 0x200000, 0x100000, 0x080000, 0x040000, 0x020000, 0x010000,
            // 0x008000, 0x004000, 0x002000, 0x001000, 0x000800, 0x000400, 0x000200, 0x000100,
            // 0x000080, 0x000040, 0x000020, 0x000010, 0x000008, 0x000004, 0x000002, 0x000001
        } ;
        return Syndrome[Bit];
    }

    static uint8_t FindCRCsyndrome(uint32_t Syndr)              // quick search for a single-bit CRC syndrome
    {
        const uint16_t PacketBytes = 24;
        const uint16_t PacketBits = PacketBytes*8;
        const uint32_t Syndrome[PacketBits] =
        {
            0x000001BF, 0x000002BE, 0x000004BD, 0x000008BC, 0x000010BB, 0x000020BA, 0x000040B9, 0x000080B8,
            0x000100B7, 0x000200B6, 0x000400B5, 0x000800B4, 0x001000B3, 0x001C1BA6, 0x002000B2, 0x003836A5,
            0x004000B1, 0x00706CA4, 0x008000B0, 0x00E0D8A3, 0x010000AF, 0x01856788, 0x01C1B0A2, 0x020000AE,
            0x030ACE87, 0x038360A1, 0x040000AD, 0x049C816D, 0x06159C86, 0x0706C0A0, 0x080000AC, 0x0939026C,
            0x099DED1E, 0x0AF3852D, 0x0B0E2F5A, 0x0B60CF39, 0x0C2B3885, 0x0DD44197, 0x0E0D809F, 0x100000AB,
            0x1272046B, 0x133BDA1D, 0x15B82D7E, 0x15E70A2C, 0x161C5E59, 0x16C19E38, 0x1826E724, 0x18567084,
            0x1BA88296, 0x1C1B009E, 0x1C9AF551, 0x200000AA, 0x24E4086A, 0x2677B41C, 0x26B27D12, 0x2B705A7D,
            0x2BCE142B, 0x2BFD538F, 0x2C38BC58, 0x2D833C37, 0x304DCE23, 0x30A95D03, 0x30ACE083, 0x34170561,
            0x365B594D, 0x37510495, 0x38132373, 0x3836009D, 0x3935EA50, 0x3E44099A, 0x3F6D1170, 0x400000A9,
            0x457C2992, 0x4701E776, 0x49C81069, 0x4CEF681B, 0x4D2EC14A, 0x4D64FA11, 0x4E5C9B66, 0x4FABBF32,
            0x531B250C, 0x56E0B47C, 0x579C282A, 0x57FAA68E, 0x58717857, 0x5A086F41, 0x5A5C4545, 0x5B067836,
            0x5F4ACF3D, 0x5F4C215E, 0x609B9C22, 0x6152BA02, 0x6159C082, 0x62696516, 0x6308D707, 0x67675B18,
            0x682E0A60, 0x696A1347, 0x6B3BCB0E, 0x6CB6B24C, 0x6EA20894, 0x70264672, 0x706C009C, 0x726BD44F,
            0x72F8C363, 0x73D75505, 0x76519D14, 0x78DBBF53, 0x7A930980, 0x7ABEE100, 0x7C881299, 0x7D41E32F,
            0x7D9A7920, 0x7EDA226F, 0x800000A8, 0x80665F8A, 0x82C48D5C, 0x82DF353B, 0x860EBF26, 0x8AF85291,
            0x8E03CE75, 0x91C77F78, 0x93902068, 0x93EDE934, 0x96851D43, 0x97D5B53F, 0x98C53309, 0x99DED01A,
            0x9A5D8249, 0x9AC9F410, 0x9CB93665, 0x9E31E955, 0x9F577E31, 0xA01E918C, 0xA184A928, 0xA476D97A,
            0xA6364A0B, 0xADC1687B, 0xAF385029, 0xAFF54C8D, 0xB0E2F056, 0xB3989D0A, 0xB410DE40, 0xB4B88A44,
            0xB60CF035, 0xB719BB79, 0xBCFD5B27, 0xBE959E3C, 0xBE98425D, 0xBFC92B8B, 0xC1373821, 0xC15AF530,
            0xC2A57401, 0xC2B38081, 0xC397DB54, 0xC4D2CA15, 0xC611AE06, 0xC6866564, 0xCA67E10F, 0xCB4F0D48,
            0xCC49A919, 0xCE7E6F08, 0xCECEB617, 0xD05C145F, 0xD05F633E, 0xD2D42646, 0xD2FE3342, 0xD677960D,
            0xD82FDB33, 0xD8D44967, 0xD96D644B, 0xDC7AF777, 0xDD441093, 0xE04C8C71, 0xE0D8009B, 0xE3F39574,
            // 0xE4D7A84E, 0xE5F18662, 0xE7AEAA04, 0xEA04AD90, 0xECA33A13, 0xF1B77E52, 0xF3E97725, 0xF526127F,
            // 0xF9102498, 0xFA4A633A, 0xFA7D135B, 0xFA83C62E, 0xFB34F21F, 0xFDB4446E, 0xFF38B789, 0xFFF409A7
        } ;

        uint16_t Bot=0;
        uint16_t Top=PacketBits;
        uint32_t MidSyndr=0;
        for( ; ; )
        {
            uint16_t Mid=(Bot+Top)>>1;
            MidSyndr = Syndrome[Mid]>>8;
            if(Syndr==MidSyndr) return (uint8_t)Syndrome[Mid];
            if(Mid==Bot) break;
            if(Syndr< MidSyndr) Top=Mid;
            else Bot=Mid;
        }
        return 0xFF;
    }

    static int Correct_(uint8_t *PktData, uint8_t *PktErr, const int MaxBadBits=6) // correct the manchester-decoded packet with dead/weak bits marked
    {
        const int Bytes=24;
        uint32_t CRC = checkPI(PktData, Bytes);
        if(CRC==0)
        {
            return 0;
        }
        uint8_t ErrBit=FindCRCsyndrome(CRC);
        if(ErrBit!=0xFF)
        {
            FlipBit(PktData, ErrBit);
            // printf(":ErrBit %d\n", ErrBit);
            return 1;
        }

        uint8_t BadBitIdx[MaxBadBits];                                    // bad bit index
        uint8_t BadBitMask[MaxBadBits];                                   // bad bit mask
        uint32_t Syndrome[MaxBadBits];                                   // bad bit mask
        uint8_t BadBits=0;                                                // count the bad bits
        for(uint8_t ByteIdx=0; ByteIdx<Bytes; ByteIdx++)                  // loop over bytes
        {
            uint8_t Byte=PktErr[ByteIdx];
            uint8_t Mask=0x80;
            for(uint8_t BitIdx=0; BitIdx<8; BitIdx++)                       // loop over bits
            {
                if(Byte&Mask)
                {
                    if(BadBits<MaxBadBits)
                    {
                        BadBitIdx[BadBits]=ByteIdx;                               // store the bad bit index
                        BadBitMask[BadBits]=Mask;
                        Syndrome[BadBits]=CRCsyndrome(ByteIdx*8+BitIdx);
                    }
                    BadBits++;
                }
                Mask>>=1;
            }
            if(BadBits>MaxBadBits) break;
        }
        if(BadBits>MaxBadBits)
        {
            return -1;                                 // return failure when too many bad bits
        }
        uint8_t Loops = 1<<BadBits;
        uint8_t PrevGrayIdx=0;
        for(uint8_t Idx=1; Idx<Loops; Idx++)                              // loop through all combination of bad bit flips
        {
            uint8_t GrayIdx= Idx ^ (Idx>>1);                                // use Gray code to change flip just one bit at a time
            uint8_t BitExp = GrayIdx^PrevGrayIdx;
            uint8_t Bit=0;
            while(BitExp>>=1)
            {
                Bit++;
            }
            PktData[BadBitIdx[Bit]]^=BadBitMask[Bit];
            CRC^=Syndrome[Bit];
            if(CRC==0)
            {
                return Count1s(GrayIdx);
            }
            uint8_t ErrBit=FindCRCsyndrome(CRC);
            if(ErrBit!=0xFF)
            {
                FlipBit(PktData, ErrBit);
                return Count1s(GrayIdx)+1;
            }
            PrevGrayIdx=GrayIdx;
        }
        return -1;
    }
}  __attribute__ ((packed));
