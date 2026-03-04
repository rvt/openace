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
 * @brief PacketBufferBase stores complete packets in a static buffer and tracks
 * fetching data will only return whole packets
 *
 * @tparam SIZE       Total size of the internal buffer in bytes.
 * @tparam MAXENTRIES Maximum number of packets that can be tracked at once.
 */

template <typename Derived>
class PacketBufferBase
{
protected:
    uint8_t *bufferData;
    size_t bufferSize;
    size_t writePosition;

    PacketBufferBase(uint8_t *buf, size_t size)
        : bufferData(buf), bufferSize(size), writePosition(0) {}

    auto &entries() { return static_cast<Derived *>(this)->entriesImpl(); }
    const auto &entries() const { return static_cast<const Derived *>(this)->entriesImpl(); }

    size_t calculateWritePos() const
    {
        if (!entries().empty())
        {
            auto &last = entries().back();
            return static_cast<size_t>(last.data() + last.size() - bufferData);
        }
        return 0;
    }

    // @deprecated
    void compact2()
    {
        if (entries().empty())
        {
            writePosition = 0;
            return;
        }

        size_t dst = 0;
        for (auto &sp : entries())
        {
            if (sp.data() != bufferData + dst)
            {
                etl::move(sp.begin(), sp.end(), bufferData + dst);
                sp = {bufferData + dst, sp.size()};
            }
            dst += sp.size();
        }

        writePosition = calculateWritePos();
    }

public:
    bool setString(const etl::string_view sv)
    {
        auto s = etl::span<const uint8_t>(reinterpret_cast<const uint8_t *>(sv.data()), sv.size());
        return set(s);
    }

    bool set(const etl::span<const uint8_t> newPacket)
    {
        if (entries().full())
            return false;

        if (writePosition + newPacket.size() > bufferSize)
            return false;

        etl::copy(newPacket.begin(), newPacket.end(), bufferData + writePosition);
        entries().push_back({bufferData + writePosition, newPacket.size()});
        writePosition += newPacket.size();

        return true;
    }

    void compact()
    {
        if (entries().empty())
        {
            writePosition = 0;
            return;
        }

        auto firstPtr = entries().front().data();
        size_t usedBytes = writePosition - static_cast<size_t>(firstPtr - bufferData);

        if (firstPtr != bufferData)
        {
            etl::move(firstPtr, firstPtr + usedBytes, bufferData);
            for (auto &sp : entries())
            {
                sp = {bufferData + (sp.data() - firstPtr), sp.size()};
            }
        }

        writePosition = usedBytes;
    }

    void clear()
    {
        entries().clear();
        writePosition = 0;
    }

    /**
     * Read data up untill a maximum length, only whole packets will be returned
     * Data will stay in the buffer and will not be overwritten unless compact is called
     */
    bool read(etl::span<uint8_t> &output, size_t maxSize = SIZE_MAX)
    {
        auto data = read(maxSize);
        if (data)
        {
            output = *data;
            return true;
        }
        return false;
    }

    /**
     * @sa bool read(etl::span<uint8_t> &output, size_t maxSize = SIZE_MAX)
     * When maxSize is set to 1, we will always remove the packet
     * TODO: Check if we remove a packet if it's larger tgahan maxSize to ensure we always continue
     */
    etl::optional<etl::span<uint8_t>> read(size_t maxSize = SIZE_MAX)
    {
        if (entries().empty())
        {
            return etl::nullopt;
        }

        size_t totalSize = 0;
        size_t idx = 0;
        while (totalSize < maxSize && idx < entries().size())
        {
            auto nextSize = totalSize + entries()[idx].size();
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

        auto span = entries().front();
        while (idx--)
        {
            entries().erase(entries().begin());
        }

        // When requested size is smaller then given we guaranteed the package is removed, but not used
        // TODO: Add test for this
        if (totalSize > maxSize)
        {
            return etl::nullopt;
        }
        return etl::span<uint8_t>{span.data(), totalSize};
    }

    /**
     * Just take the first item if any
     * TODO: Make test
     */
    etl::optional<etl::span<uint8_t>> take()
    {
        if (entries().empty())
        {
            return etl::nullopt;
        }
        auto span = entries().front();
        entries().erase(entries().begin());
        return span;
    }

    size_t packets() const { return entries().size(); }
    size_t used() const { return writePosition; }
    size_t remaining() const { return bufferSize - writePosition; }
    uint8_t persFull() const { return writePosition * 100 / bufferSize; }
};

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
template <size_t SIZE, size_t MAXENTRIES>
class PacketBuffer : public PacketBufferBase<PacketBuffer<SIZE, MAXENTRIES>>
{
    friend class PacketBufferBase<PacketBuffer<SIZE, MAXENTRIES>>;

private:
    uint8_t buffer[SIZE];
    etl::vector<etl::span<uint8_t>, MAXENTRIES> entryVec;

    auto &entriesImpl() { return entryVec; }
    const auto &entriesImpl() const { return entryVec; }

public:
    // Now buffer is constructed before the base class, so buffer.data() is safe
    PacketBuffer()
        : PacketBufferBase<PacketBuffer<SIZE, MAXENTRIES>>(buffer, SIZE) {}
};
