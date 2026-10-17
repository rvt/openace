#pragma once
#include "ace/deviceactions.hpp"
#include "ace/cobs.hpp"
#include "etl/bit_stream.h"
#include "etl/optional.h"

namespace GATAS
{
    // The fixed, big-endian wire layout is shared with Companion. These frames
    // fit the existing COBS transport; legacy message layouts never change.
    struct DeviceActionCodec
    {
        static constexpr uint8_t REQUEST = 12;
        static constexpr uint8_t RESPONSE = 13;
        static constexpr uint8_t ALL_CAPABILITIES = DEVICE_ACTION_CAPABILITIES; // Save, Restart, USB boot.
        static constexpr size_t REQUEST_SIZE = 16;
        static constexpr size_t RESPONSE_SIZE = 17;

        static etl::optional<DeviceActionRequest> decode(etl::span<const uint8_t> raw)
        {
            if (raw.size() != REQUEST_SIZE || raw[0] != REQUEST)
            {
                return etl::nullopt;
            }
            etl::bit_stream_reader reader(raw, etl::endian::big);
            reader.read_unchecked<uint8_t>();
            DeviceActionRequest request;
            request.action = static_cast<DeviceAction>(reader.read_unchecked<uint8_t>());
            request.phase = static_cast<DeviceActionPhase>(reader.read_unchecked<uint8_t>());
            request.requestId = reader.read_unchecked<uint32_t>();
            request.bootId = reader.read_unchecked<uint64_t>();
            request.updatePolicy = static_cast<DeviceUpdatePolicy>(reader.read_unchecked<uint8_t>());
            return request;
        }

        static size_t encode(const DeviceActionResponse &response, uint8_t *out, size_t capacity)
        {
            uint8_t raw[RESPONSE_SIZE];
            etl::bit_stream_writer writer(raw, sizeof(raw), etl::endian::big);
            writer.write_unchecked<uint8_t>(RESPONSE);
            writer.write_unchecked<uint8_t>(static_cast<uint8_t>(response.action));
            writer.write_unchecked<uint32_t>(response.requestId);
            writer.write_unchecked<uint8_t>(static_cast<uint8_t>(response.result));
            writer.write_unchecked<uint8_t>(response.capabilities);
            writer.write_unchecked<uint64_t>(response.bootId);
            writer.write_unchecked<uint8_t>(response.dirty ? 1 : 0);
            return encodeCOBS(raw, sizeof(raw), out, capacity, true);
        }
    };

}
