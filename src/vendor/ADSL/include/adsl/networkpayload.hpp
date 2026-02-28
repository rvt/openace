#pragma once

#include <stdint.h>

#include "etl/algorithm.h"
#include "etl/bit_stream.h"

namespace ADSL
{

  /**
   * @brief ADS-L.4.SRD860.E.1 Network Packet Structure
   *
   * The network layer governs:
   * - Protocol versioning
   * - Optional secure signature for sender authentication
   * - Data scrambling and error control
   */
  class NetworkPayload final
  {
  public:
    static constexpr uint8_t TotalHeaderBits = 8; // Total bits in the network header
    struct ProtocolVersion
    {
      enum enum_type : uint8_t
      {
        ISSUE_1 = 0,
        ISSUE_2 = 1
      };

      ETL_DECLARE_ENUM_TYPE(ProtocolVersion, uint8_t)
      ETL_ENUM_TYPE(ISSUE_1, "Issue 1")
      ETL_ENUM_TYPE(ISSUE_2, "Issue 2")
      ETL_END_ENUM_TYPE

    public:
      static constexpr enum_type EARLIEST = ISSUE_1;
      static constexpr enum_type LATEST = ISSUE_2;
    };

    struct ErrorControlMode
    {
      enum enum_type : uint8_t
      {
        CRC = 0,
        FEC = 1
      };

      ETL_DECLARE_ENUM_TYPE(ErrorControlMode, uint8_t)
      ETL_ENUM_TYPE(CRC, "CRC")
      ETL_ENUM_TYPE(FEC, "FEC")
      ETL_END_ENUM_TYPE
    };

  private:
    ProtocolVersion protocolVersion_ = ProtocolVersion::ISSUE_2;
    bool secureSignatureFlag_ = false;                          // 1 bit
    uint8_t keyIndex_ = 0;                                      // 2 bits
    ErrorControlMode errorControlMode_ = ErrorControlMode::CRC; // 1 bit: 0=CRC, 1=FEC

  public:
    explicit NetworkPayload() = default;
    constexpr NetworkPayload(ProtocolVersion pv,
                             bool ssf,
                             uint8_t ki,
                             ErrorControlMode ecm)
        : protocolVersion_(pv), secureSignatureFlag_(ssf), keyIndex_(ki), errorControlMode_(ecm)
    {
    }

    /**
     * @brief Get Protocol Version (4 bits)
     */
    ProtocolVersion protocolVersion() const { return protocolVersion_; }

    void protocolVersion(ProtocolVersion p) { protocolVersion_ = p; }

    /**
     * @brief Get Secure Signature Flag
     * 0: No signature present
     * 1: Signature present
     */
    bool secureSignatureFlag() const { return secureSignatureFlag_; }
    void secureSignatureFlag(bool b) { secureSignatureFlag_ = b; }

    /**
     * @brief Get Key Index (2 bits)
     * Designates the key used, 0 for public 3 ke
     */
    uint8_t keyIndex() const
    {
      return keyIndex_;
    }

    void keyIndex(uint8_t index)
    {
      keyIndex_ = etl::clamp(index, static_cast<uint8_t>(0x00), static_cast<uint8_t>(0x03));
    }

    /**
     * @brief Set Error Control Mode
     * @param mode true for FEC, false for CRC
     */
    void errorControlMode(ErrorControlMode mode) { errorControlMode_ = mode; }

    ErrorControlMode errorControlMode() const { return errorControlMode_; }

    /**
     * @brief Serialize the Network Header to a bit stream. v1/v2 are the same
     * Total width: 8 bits (1 byte)
     * @param writer The bit stream writer.
     */
    void serialize_issue1(etl::bit_stream_writer &writer) const
    {
      writer.write_unchecked(static_cast<uint8_t>(protocolVersion_), 4U);
      writer.write_unchecked(secureSignatureFlag_);
      writer.write_unchecked(keyIndex_, 2U);
      writer.write_unchecked(static_cast<uint8_t>(errorControlMode_), 1U);
    }

    /**
     * @brief Get the serialized Network Header as a uint8_t.
     * Note: Ensure this works on your platform! The serialise function is preferred as they are platform independent
     * @return The serialized Network Header.
     */
    uint8_t asUint8() const
    {
      uint8_t value = 0;
      value |= (static_cast<uint8_t>(protocolVersion_) & 0x0F);
      value |= (secureSignatureFlag_ ? 1 : 0) << 4;
      value |= (keyIndex_ & 0x03) << 5;
      value |= (static_cast<uint8_t>(errorControlMode_) & 0x01) << 7;
      return value;
    }

    /**
     * @brief Populate fields from a serialized network header byte.
     * Mirrors the bit layout used in serialize_issue1 / asUint8:
     * - bits 0..3: protocolVersion
     * - bit 4: secureSignatureFlag
     * - bits 5..6: keyIndex
     * - bit 7: errorControlMode
     */
    static NetworkPayload fromUint8(uint8_t value)
    {
      return {
          static_cast<ProtocolVersion>(value & 0x0F),
          (value & 0x10) != 0,
          static_cast<uint8_t>((value >> 5) & 0x03),
          static_cast<ErrorControlMode>((value >> 7) & 0x01)};
    }

    /**
     * @brief Deserialize the Network Header from a bit stream.  v1/v2 are the same
     * @param reader The bit stream reader.
     * @return The deserialized NetworkPayload object.
     */
    static NetworkPayload deserialize_issue1(etl::bit_stream_reader &reader)
    {
      NetworkPayload network;
      network.protocolVersion_ = static_cast<ProtocolVersion>(reader.read_unchecked<uint8_t>(4U));
      network.secureSignatureFlag_ = reader.read_unchecked<bool>();
      network.keyIndex_ = reader.read_unchecked<uint8_t>(2U);
      network.errorControlMode_ = static_cast<ErrorControlMode>(reader.read_unchecked<uint8_t>(1U));
      return network;
    }
  };

} // namespace ADSL
