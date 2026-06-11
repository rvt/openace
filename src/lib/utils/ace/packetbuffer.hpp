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
 * @brief CRTP helper for a fixed-capacity FIFO of packet spans.
 *
 * The derived class provides the backing storage and the packet index vector.
 * This base class:
 * - copies each packet into the fixed buffer once on insert
 * - keeps a span for each stored packet
 * - only returns complete packets from read()/take()
 * - does not reclaim space automatically; call compact() to move unread data
 *   back to the front of the buffer
 *
 * @tparam Derived Concrete packet buffer type that exposes entriesImpl().
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

public:
    bool setString(const etl::string_view sv)
    {
        return set({reinterpret_cast<const uint8_t *>(sv.data()), sv.size()});
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
     * Read the oldest packet or packet group whose total size fits within maxSize.
     *
     * The returned span aliases the internal buffer. The packet entries are removed
     * from the queue, but the bytes remain in place until compact() is called or the
     * buffer is overwritten by later inserts.
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
     * Read and remove the oldest packet group.
     *
     * The returned span refers to the internal buffer and contains only complete
     * packets whose combined size does not exceed maxSize.
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
     * Remove and return the oldest stored packet without any size filtering.
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
 * @brief Concrete fixed-size packet queue.
 *
 * PacketBuffer owns the backing storage and packet index vector for
 * PacketBufferBase. It is intended for embedded use where dynamic allocation
 * is undesirable and packets need to be queued as contiguous spans.
 *
 * The buffer accepts arbitrary byte sequences, stores them contiguously, and
 * exposes them again as spans in FIFO order.
 *
 * @tparam SIZE Fixed internal buffer size in bytes.
 * @tparam MAXENTRIES Maximum number of packets that can be queued.
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
