#pragma once
#include <cstddef>
#include <cstdint>
#include <etl/algorithm.h>
#include <etl/span.h>

/**
 * @brief StreamBuffer is a small-footprint buffer for assembling and splitting
 *        COBS (or similar) framed messages with minimal memory copying.
 *
 * This implementation is designed for embedded systems:
 *  - Uses a fixed-size static buffer (SIZE) — no dynamic allocation.
 *  - Stores incoming partial packets until a separator (SEPARATOR) is found.
 *  - Avoids unnecessary memcpy by:
 *      * Using spans to reference existing data.
 *      * Only copying when partial data must be appended.
 *  - Ensures safe handling of packets larger than buffer capacity by discarding
 *    oversized data instead of corrupting state.
 *
 * Typical use case:
 *  - Bets situation to use is when you have larger datasizes that needs to be parsed but with a
 *    chance of left over bytes that needs to be parsed on subsequent calls
 *  - Call set() whenever new bytes arrive.
 *  - Call read() to extract completed packets in order, as long as it returns true, keep reading the data
 *  - peekBuffer() / peekPacket() allow inspection without removing data.
 *
 * @tparam SIZE      Fixed internal buffer capacity in bytes, must be minimal the size dataset of some maximum size 
 *        
 * @tparam SEPARATOR Byte value used to mark end of a packet (e.g., 0 for COBS).
 */
template <size_t SIZE, uint8_t SEPARATOR>
class StreamBuffer
{
private:
    etl::array<uint8_t, SIZE> buffer;     ///< Internal static buffer
    etl::span<uint8_t> bufferSpan;        ///< Active portion of internal buffer
    etl::span<uint8_t> packet;            ///< Pending packet data not yet in buffer


public:
    /// @brief Constructor — initialises with empty buffer and no packet.
    StreamBuffer() : bufferSpan(buffer.data(), 0), packet() {}

    /**
     * @brief Add new data to the stream buffer.
     *
     * If a separator is found:
     *   - Completes a packet, returning true.
     * If no separator is found:
     *   - Stores data in buffer until a complete packet is available.
     *   - Copies only what is necessary to append or start buffering.
     *
     * @param newPacket Incoming data bytes.
     * @return true if a complete packet is available after adding.
     *         false if only partial data is stored.
     */
    bool set(const etl::span<uint8_t> newPacket)
    {
        auto sepPos = etl::find(newPacket.begin(), newPacket.end(), SEPARATOR);
        packet = {};

        // Case 1: Empty buffer
        if (bufferSpan.empty())
        {
            if (sepPos != newPacket.end())
            {
                packet = newPacket; // Has full COBS packet
                return true;
            }
            if (newPacket.size() < SIZE)
            {
                etl::copy(newPacket.begin(), newPacket.end(), buffer.data());
                bufferSpan = { buffer.data(), newPacket.size() };
            }
            return false;
        }

        // Case 2: Buffer already has data
        if (sepPos != newPacket.end())
        {
            size_t combinedSize = bufferSpan.size() + (sepPos - newPacket.begin() + 1);
            if (combinedSize <= SIZE)
            {
                // Append up to separator
                etl::copy(newPacket.begin(), sepPos + 1, bufferSpan.end());
                bufferSpan = { buffer.data(), combinedSize };
                // Remaining goes into packet
                packet = { sepPos + 1, static_cast<size_t>(newPacket.end() - (sepPos + 1)) };
                return true;
            }
            // Doesn't fit — flush buffer, take as packet
            bufferSpan = { buffer.data(), 0 };
            packet = newPacket;
            return true;
        }

        // No separator in new packet
        if (newPacket.size() + bufferSpan.size() < SIZE)
        {
            etl::copy(newPacket.begin(), newPacket.end(), bufferSpan.end());
            bufferSpan = { buffer.data(), bufferSpan.size() + newPacket.size() };
        }
        else if (newPacket.size() < SIZE)
        {
            etl::copy(newPacket.begin(), newPacket.end(), buffer.data());
            bufferSpan = { buffer.data(), newPacket.size() };
        }
        else
        {
            // Too large — reset
            bufferSpan = {};
        }
        return true;
    }

    /// @brief Clears both internal buffer and pending packet references.
    void clear()
    {
        bufferSpan = {};
        packet = {};
    }

    /// @brief Access current buffered (incomplete) data without removing it.
    auto peekBuffer() const { return bufferSpan; }

    /// @brief Access current packet data without removing it.
    auto peekPacket() const { return packet; }

    /**
     * @brief Reads the next complete packet op to separatr if available.
     *
     * This checks both the internal buffer and any pending packet data.
     * On a successful read:
     *   - The output span references the packet data directly.
     *   - The internal buffer or packet reference is updated to remove
     *     the returned packet.
     *
     * @param output Span reference to receive the packet view.
     * @return true if a complete packet was returned, false otherwise.
     */
    bool read(etl::span<uint8_t>& output)
    {
        if (!bufferSpan.empty())
        {
            auto sepPos = etl::find(bufferSpan.begin(), bufferSpan.end(), SEPARATOR);
            if (sepPos != bufferSpan.end())
            {
                output = bufferSpan;
                bufferSpan = {};
                return true;
            }
            output = {};
            return false;
        }

        auto sepPos = etl::find(packet.begin(), packet.end(), SEPARATOR);
        if (sepPos == packet.end())
        {
            if (packet.size() < SIZE)
            {
                etl::copy(packet.begin(), packet.end(), buffer.data());
                bufferSpan = { buffer.data(), packet.size() };
            }
            packet = {};
            output = {};
            return false;
        }

        // Found separator in packet
        output = { packet.begin(), sepPos + 1 };
        packet = { sepPos + 1, static_cast<size_t>(packet.end() - (sepPos + 1)) };
        return true;
    }
};
