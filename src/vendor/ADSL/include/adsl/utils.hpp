#pragma once

#include <cmath>
#include <stdint.h>
#include <inttypes.h>

#include <etl/algorithm.h>
#include <etl/math.h>
#include <etl/power.h>
#include <etl/ratio.h>
#include <etl/memory.h>
#include <etl/span.h>

namespace ADSL
{
  constexpr float DEG_TO_RADS = M_PI / 180.f; // degrees to radians

  template <class Type, int Bits>
  Type UnsVRdecode(Type Value)
  {
    const Type Thres = 1 << Bits;
    uint8_t Range = Value >> Bits;
    Value &= Thres - 1;
    if (Range == 0)
      return Value;
    if (Range == 1)
      return Thres + 1 + (Value << 1);
    if (Range == 2)
      return 3 * Thres + 2 + (Value << 2);
    return 7 * Thres + 4 + (Value << 3);
  }

  template <class Type, int Bits>
  Type UnsVRencode(Type Value)
  {
    const Type Thres = 1 << Bits;
    if (Value < Thres)
      return Value;
    if (Value < 3 * Thres)
      return Thres | ((Value - Thres) >> 1);
    if (Value < 7 * Thres)
      return 2 * Thres | ((Value - 3 * Thres) >> 2);
    if (Value < 15 * Thres)
      return 3 * Thres | ((Value - 7 * Thres) >> 3);
    return 4 * Thres - 1;
  }

  template <class Type, int Bits>
  Type SignVRencode(Type Value)
  {
    const Type SignMask = 1 << (Bits + 2);
    Type Sign = 0;
    if (Value < 0)
    {
      Value = (-Value);
      Sign = SignMask;
    }
    Value = UnsVRencode<Type, Bits>(Value);
    return Value | Sign;
  }

  template <class Type, int Bits>
  Type SignVRdecode(Type Value)
  {
    const Type SignMask = 1 << (Bits + 2);
    Type Sign = Value & SignMask;
    Value = UnsVRdecode<Type, Bits>(Value & (SignMask - 1));
    return Sign ? -Value : Value;
  }

  static constexpr float FNTtoFloatConf = 90.0007295677 / 0x40000000;

  inline float FNTtoFloat(int32_t Coord)
  {
    return FNTtoFloatConf * Coord;
  }

  inline int32_t FloatToFNT(float Coord)
  {
    return Coord / FNTtoFloatConf;
  }

  inline float wrapLonDelta(float dLon)
  {
    // Wrap to [-180, +180]
    if (dLon > 180.0f)
    {
      dLon -= 360.0f;
    }
    if (dLon < -180.0f)
    {
      dLon += 360.0f;
    }
    return dLon;
  }

  struct relNorthRelEast
  {
    float north;
    float east;
  };

  inline relNorthRelEast northEastDistance(float fromLat, float fromLon, float toLat,
                                           float toLon)
  {

    float dLat = toLat - fromLat;
    float dLon = toLon - fromLon;

    float kx = cosf(fromLat * DEG_TO_RADS) * 111321.0f;

    float dx = wrapLonDelta(dLon) * kx;
    float dy = dLat * 111139.0f;

    return {dy, dx};
  }

  /**
   * Calculate the distance between two points on earth fast
   *
   * Note: Distance in what we are interested in <30Km at lat 30 degrees the
   * difference is about 15m
   *
   * returns distance in meters
   * https://jamesloper.com/fastest-way-to-calculate-distance-between-two-coordinates
   * https://www.movable-type.co.uk/scripts/latlong.html
   */
  inline float distanceFast(float fromLat, float fromLon, float toLat, float toLon)
  {
    auto ne = northEastDistance(fromLat, fromLon, toLat, toLon);
    return sqrtf((ne.north * ne.north) + (ne.east * ne.east));
  }

  inline relNorthRelEast velocityFromTrack(float trackDeg, float speed)
  {
    float tr = trackDeg * DEG_TO_RADS;

    return {.north = cosf(tr) * speed, .east = sinf(tr) * speed};
  }

  /**
   * Compute TCPA (Closest Point of Approach) in seconds.
   */
  inline float computeTCPA(relNorthRelEast relativeDistance, float ownTrackDeg,
                           float ownSpeed, float tgtTrackDeg, float tgtSpeed)
  {

    // Velocities
    relNorthRelEast vOwn = velocityFromTrack(ownTrackDeg, ownSpeed);
    relNorthRelEast vTgt = velocityFromTrack(tgtTrackDeg, tgtSpeed);

    // Relative velocity
    relNorthRelEast v;
    v.east = vTgt.east - vOwn.east;
    v.north = vTgt.north - vOwn.north;

    float v2 = v.east * v.east + v.north * v.north;

    // If relative speed is basically zero → no CPA
    if (v2 < 0.1f)
    {
      return INFINITY;
    }

    // tcpa = - (r · v) / |v|²
    float dot = relativeDistance.east * v.east + relativeDistance.north * v.north;
    float tcpa = -dot / v2;

    if (tcpa < 0)
    {
      return INFINITY;
    }
    return tcpa;
  }

  inline bool isUsReached(uint32_t referenceUs, uint32_t now)
  {
    return static_cast<int32_t>(referenceUs - now) < 0;
  }

  inline uint32_t XXTEA_MX_KEY0(uint32_t Y, uint32_t Z, uint32_t Sum)
  {
    return ((((Z >> 5) ^ (Y << 2)) + ((Y >> 3) ^ (Z << 4))) ^ ((Sum ^ Y) + Z));
  }

  constexpr uint32_t XXTEADELTA = 0x9e3779b9;
  inline void XXTEA_Encrypt_Key0(uint32_t Data[], uint8_t Words, uint8_t Loops)
  {
    uint32_t Sum = 0;
    uint32_t Z = Data[Words - 1];
    uint32_t Y;
    for (; Loops; Loops--)
    {
      Sum += XXTEADELTA;
      for (uint8_t P = 0; P < (Words - 1); P++)
      {
        Y = Data[P + 1];
        Z = Data[P] += XXTEA_MX_KEY0(Y, Z, Sum);
      }
      Y = Data[0];
      Z = Data[Words - 1] += XXTEA_MX_KEY0(Y, Z, Sum);
    }
  }

  inline void XXTEA_Decrypt_Key0(uint32_t Data[], uint8_t Words, uint8_t Loops)
  {
    uint32_t Sum = Loops * XXTEADELTA;
    uint32_t Y = Data[0];
    uint32_t Z;
    for (; Loops; Loops--)
    {
      for (uint8_t P = Words - 1; P; P--)
      {
        Z = Data[P - 1];
        Y = Data[P] -= XXTEA_MX_KEY0(Y, Z, Sum);
      }
      Z = Data[Words - 1];
      Y = Data[0] -= XXTEA_MX_KEY0(Y, Z, Sum);
      Sum -= XXTEADELTA;
    }
  }

  /**
   * From: ADS-L.4.SRD860.E.3.1
   */
  inline uint32_t crc24_polypass(uint32_t crc, uint8_t input) // pass a single byte through the CRC polynomial
  {
    const uint32_t poly = 0xFFFA0480;
    crc |= input;
    for (uint8_t Bit = 0; Bit < 8; Bit++)
    {
      if (crc & 0x80000000)
      {
        crc ^= poly;
      }
      crc <<= 1;
    }
    return crc;
  }

  inline uint32_t checkPI(etl::span<const uint8_t> bytes)
  {
    uint32_t crc = 0;
    for (auto b : bytes)
    {
      crc = crc24_polypass(crc, b);
    }
    return crc >> 8;
  }

  inline uint32_t calcPI(etl::span<const uint8_t> bytes)
  {
    uint32_t crc = 0;
    for (uint8_t b : bytes)
    {
      crc = crc24_polypass(crc, b);
    }
    crc = crc24_polypass(crc, 0);
    crc = crc24_polypass(crc, 0);
    crc = crc24_polypass(crc, 0);

    return crc >> 8;
  }

  inline void FlipBit(etl::span<uint8_t> Byte, int BitIdx)
  {
    int ByteIdx = BitIdx >> 3;
    BitIdx &= 7;
    BitIdx = 7 - BitIdx;
    uint8_t Mask = 1;
    Mask <<= BitIdx;
    Byte[ByteIdx] ^= Mask;
  }

  inline uint8_t Count1s(uint8_t Byte)
  {
    return __builtin_popcount(Byte);
  }

    inline uint8_t FindLowestSetBit(uint8_t val)
  {
    return __builtin_ctz(val);
  }

  inline uint32_t CRCsyndrome(size_t Bit, size_t size)
  {
    const uint16_t PacketBytes = 24;
    // Fow now only support 24byte
    if (Bit > size * 8)
    {
      return 0;
    }

    const uint16_t PacketBits = PacketBytes * 8;
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
            0x008000, 0x004000, 0x002000, 0x001000, 0x000800, 0x000400, 0x000200, 0x000100,
            0x000080, 0x000040, 0x000020, 0x000010, 0x000008, 0x000004, 0x000002, 0x000001};
    return Syndrome[Bit];
  }

  inline uint8_t FindCRCsyndrome(uint32_t Syndr, size_t size) // quick search for a single-bit CRC syndrome
  {
    const uint16_t PacketBytes = 24;
    // Fow now only support 24byte
    if (size != PacketBytes)
    {
      return 0xFF;
    }

    const uint16_t PacketBits = PacketBytes * 8;
    const uint32_t Syndrome[] =
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
            0xE4D7A84E, 0xE5F18662, 0xE7AEAA04, 0xEA04AD90, 0xECA33A13, 0xF1B77E52, 0xF3E97725, 0xF526127F,
            0xF9102498, 0xFA4A633A, 0xFA7D135B, 0xFA83C62E, 0xFB34F21F, 0xFDB4446E, 0xFF38B789, 0xFFF409A7};

    uint16_t Bot = 0;
    uint16_t Top = PacketBits;
    uint32_t MidSyndr = 0;
    for (;;)
    {
      uint16_t Mid = (Bot + Top) >> 1;
      MidSyndr = Syndrome[Mid] >> 8;
      if (Syndr == MidSyndr)
        return (uint8_t)Syndrome[Mid];
      if (Mid == Bot)
        break;
      if (Syndr < MidSyndr)
        Top = Mid;
      else
        Bot = Mid;
    }
    return 0xFF;
  }

  inline int Correct(etl::span<uint8_t> PktData, etl::span<const uint8_t> PktErr, int MaxBadBits = 6)
  {
    const uint32_t pktSize = PktData.size();

    uint32_t CRC = checkPI(PktData);
    if (CRC == 0)
    {
      return 0;
    }

    uint8_t ErrBit = FindCRCsyndrome(CRC, pktSize);
    if (ErrBit != 0xFF)
    {
      FlipBit(PktData, ErrBit);
      return 1;
    }

    uint8_t BadBitIdx[MaxBadBits];
    uint8_t BadBitMask[MaxBadBits];
    uint32_t Syndrome[MaxBadBits];
    uint32_t BadBits = 0;

    for (uint8_t ByteIdx = 0; ByteIdx < pktSize; ByteIdx++)
    {
      const uint8_t Byte = PktErr[ByteIdx];
      if (!Byte)
        continue; // skip clean bytes - important on slow flash

      for (uint8_t BitIdx = 0; BitIdx < 8; BitIdx++)
      {
        if (Byte & (0x80u >> BitIdx))
        {
          if (BadBits < MaxBadBits)
          {
            BadBitIdx[BadBits] = ByteIdx;
            BadBitMask[BadBits] = 0x80u >> BitIdx;
            Syndrome[BadBits] = CRCsyndrome(ByteIdx * 8 + BitIdx, pktSize);
          }
          if (++BadBits > MaxBadBits)
            goto search_done;
        }
      }
    }

  search_done:
    if (BadBits > MaxBadBits)
      return -1;

    const uint32_t Loops = 1u << BadBits;
    uint32_t PrevGrayIdx = 0;

    for (uint8_t Idx = 1; Idx < Loops; Idx++)
    {
      const uint32_t GrayIdx = Idx ^ (Idx >> 1);
      const uint32_t Bit = FindLowestSetBit(GrayIdx ^ PrevGrayIdx);

      PktData[BadBitIdx[Bit]] ^= BadBitMask[Bit];
      CRC ^= Syndrome[Bit];

      if (CRC == 0)
      {
        return Count1s(GrayIdx);
      }

      ErrBit = FindCRCsyndrome(CRC, pktSize);
      if (ErrBit != 0xFF)
      {
        FlipBit(PktData, ErrBit);
        return Count1s(GrayIdx) + 1;
      }

      PrevGrayIdx = GrayIdx;
    }
    return -1;
  }

} // namespace ADSL