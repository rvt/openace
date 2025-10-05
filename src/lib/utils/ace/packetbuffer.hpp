#pragma once
#include <cstddef>
#include <cstdint>
#include <etl/algorithm.h>
#include <etl/span.h>
#include <etl/vector.h>
#include <etl/algorithm.h>
#include <etl/string_view.h>
#include <etl/optional.h>

/**
 * @brief PacketBuffer is a small-footprint buffer to store arbit
 *
 * This implementation is designed for embedded systems:
 *  - Uses a fixed-size static buffer (SIZE) — no dynamic allocation.
 *  - Stores incoming  packets
 *  - Avoids unnecessary memcpy by:
 *      * Using spans to reference existing data.
 *      * Only copy the data once
 *  - Ensures safe handling of packets larger than buffer capacity by discarding
 *
 * Typical use case:
 * - When you need to store aribiary sized data in a compact buffer instead of a list like NMEA data
 *
 * @tparam SIZE      Fixed internal buffer capacity in bytes, must be minimal the size dataset of some maximum size
 *
 * @tparam MAXENTRIES Maximum entries
 */

/**
 * @brief StreamBuffer stores complete packets in a static buffer and tracks them using spans.
 *
 * @tparam SIZE       Total size of the internal buffer in bytes.
 * @tparam MAXENTRIES Maximum number of packets that can be tracked at once.
 */
template <size_t SIZE, size_t MAXENTRIES>
class PacketBuffer
{
private:
    etl::array<uint8_t, SIZE> buffer;                    ///< Internal static buffer
    etl::vector<etl::span<uint8_t>, MAXENTRIES> entries; ///< Queue of spans pointing to packets
    size_t writePosition;

    size_t calculateWritePos() const
    {
        if (!entries.empty())
        {
            auto &last = entries.back();
            return static_cast<size_t>(last.data() + last.size() - buffer.data());
        }
        return 0;
    }

public:
    PacketBuffer() : writePosition(0) {}

    bool setString(etl::string_view sv)
    {
        auto s = etl::span<const uint8_t>(reinterpret_cast<const uint8_t *>(sv.data()), sv.size());
        return set(s);
    }

    // Add a complete packet into the buffer and track it via span
    bool set(const etl::span<const uint8_t> newPacket)
    {
        if (entries.full())
        {
#if GATAS_DEBUG == 1
            printf("PacketBuffer: No more entries %d\n", entries.size());
#endif
            return false;
        }

        // Check if buffer has enough room, else compact
        if (writePosition + newPacket.size() > SIZE)
        {
            // Don't allow to compact during set so that buffer stau constant untill compact can be called
            // compact();
            // if (writePosition + newPacket.size() > SIZE)
            // {
            //     // Still not enough space after compaction
            //     return false;
            // }

#if GATAS_DEBUG == 1
            printf("PacketBuffer: No enough space %d\n", writePosition);
#endif
            return false;
        }

        // Copy data into buffer
        etl::copy(newPacket.begin(), newPacket.end(), buffer.begin() + writePosition);

        // Push a span into entries
        entries.push_back({buffer.data() + writePosition, newPacket.size()});
        writePosition += newPacket.size();

        return true;
    }

    // Compact the buffer, moving all remaining packets to the start
    void compact()
    {
        if (entries.empty())
        {
            writePosition = 0;
            return;
        }

        size_t dst = 0;
        for (auto &sp : entries)
        {
            if (sp.data() != buffer.data() + dst)
            {
                // Move packet to the current destination position
                etl::move(sp.begin(), sp.end(), buffer.begin() + dst);
                sp = {buffer.data() + dst, sp.size()};
            }
            dst += sp.size();
        }

        writePosition = calculateWritePos();
    }

    // Clear the buffer and all entries
    void clear()
    {
        entries.clear();
        writePosition = 0;
    }

    // Note: You must copy the data before the next set or compact
    bool read(etl::span<uint8_t> &output, size_t maxSize = std::numeric_limits<size_t>::max())
    {
        auto data = read(maxSize);
        if (data)
        {
            output = *data;
            return true;
        }
        return false;
    }

    // Retrieve and remove the next packet
    // Note: You must copy the data before the next set or compact
    etl::optional<etl::span<uint8_t>> read(size_t maxSize = std::numeric_limits<size_t>::max())
    {
        if (entries.empty())
        {
            return etl::nullopt;
        }

        size_t totalSize = 0;
        size_t idx = 0;
        while (totalSize < maxSize && idx < entries.size())
        {
            auto nextSize = totalSize + entries[idx].size();
            if (nextSize <= maxSize)
            {
                totalSize = nextSize;
                idx++;
            }
            else
            {
                break;
            }
        }

        auto span = entries.front();
        while (idx--)
        {
            entries.erase(entries.begin());
        }
        return etl::span{span.data(), totalSize};
    }

    // Return number of stored packets
    size_t packets() const
    {
        return entries.size();
    }

    size_t used() const
    {
        return writePosition;
    }

    // Return remaining space in buffer
    size_t remaining() const
    {
        return SIZE - writePosition;
    }
};
