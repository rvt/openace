#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring> // for std::memmove
#include <etl/span.h>
#include <etl/vector.h>
#include <etl/algorithm.h>
#include "delimiterbitmap.hpp"

/**
 * @brief Stream packet assembler that splits an incoming byte stream into packets
 *        based on configurable delimiter characters.
 *
 * The `Gulp` class processes a continuous byte stream and extracts packets
 * delimited by one or more specified characters (e.g. '\n', '\0', etc.).
 * It supports partial data arrival by maintaining internal state between calls,
 * and can operate with both externally referenced data spans and an internal buffer.
 *
 * Typical usage:
 * @code
 * etl::vector<uint8_t, 64> buffer;
 * Gulp gulp(buffer, DelimiterBitmap::CRLF());
 *
 * // Feed incoming data fragments
 * gulp.pushAndSetRef(incoming1);
 * gulp.pushAndSetRef(incoming2);
 *
 * etl::span<uint8_t> packet;
 * while (gulp.pop_into(packet))
 * {
 *     process_packet(packet);
 * }
 * @endcode
 *
 * ## Design
 * - Incoming data is provided through `pushAndSetRef()`, which sets a new
 *   reference span (`etl::span<uint8_t>`) as the current data source.
 * - The class maintains a temporary buffer for accumulating bytes between
 *   delimiters when packets span across multiple chunks.
 * - Calling `pop_into()` extracts complete packets when available.
 *
 * ## Behavior
 * - If no complete packet is available, `pop_into()` returns `false`.
 * - When a delimiter is found, the assembled packet is provided as an
 *   `etl::span<uint8_t>` referencing internal storage.
 * - Consecutive calls continue from where the previous call left off.
 * - A zero-length or delimiter-only packet is skipped automatically.
 *
 * ## Thread Safety
 * Not thread-safe. Should only be accessed from a single context.
 *
 */
class Gulp
{
public:
    Gulp(etl::ivector<uint8_t> &buffer, const DelimiterBitmap &delimiters)
        : buffer_(buffer), delimiters_(delimiters) {}

    /**
     * When using setRef, you must call pop_into after in a loop to
     */

    void setRef(etl::span<uint8_t> ref)
    {
        currentRef_ = ref;
        refPos_ = 0;
    }

    /**
     * Copy as much of `ref` as will fit into the internal buffer and keep the ref.
     *
     * Returns true if NOT all bytes could be stored (or buffer is full) — caller must
     * call pop_into() before calling pushAndSetRef/setRef again to avoid data loss.
     * Returns false if everything fit (or ref was empty).
     */
    bool pushAndSetRef(etl::span<uint8_t> ref)
    {
        // nothing to do
        if (ref.empty())
        {
            currentRef_ = ref;
            refPos_ = 0;
            return false;
        }

        // how many bytes we can append into the internal buffer
        size_t available = buffer_.max_size() - buffer_.size();
        size_t toCopy = (ref.size() < available) ? ref.size() : available;

        buffer_.insert(buffer_.end(), ref.begin(), ref.begin() + toCopy);

        if (toCopy == ref.size())
        {
            // We copied the entire external span into the internal buffer.
            // No need to keep the external reference — parsing can continue
            // from the internal buffer only.
            currentRef_ = {};
            refPos_ = 0;

            // Return true only if the internal buffer is now full (caller must pop).
            return buffer_.full();
        }

        // We copied only part of the external span (or none). Keep the external ref,
        // but advance refPos_ so we don't re-read the bytes we already copied.
        currentRef_ = ref;
        refPos_ = toCopy;

        // We couldn't copy all bytes, caller must call pop_into() to free room (true).
        return true;
    }

    // Try to extract the next complete packet (returns false if incomplete)
    // The returned span is valid until the next modifying call (push/set/pop that mutates buffer_).
    bool pop_into(etl::span<uint8_t> &out)
    {
        out = {};
        if (!buffer_.empty())
        {
            return pop_internal(out);
        }

        return pop_external(out);
    }

    bool hasPartial() const
    {
        return !buffer_.empty();
    }

private:
    // --- helpers ---

    // 1) If internal buffer contains a delimiter, return that packet.
    //    Otherwise append bytes from currentRef_ until delimiter found or buffer full.
    bool pop_internal(etl::span<uint8_t> &out)
    {
        // Scan existing buffer first (important!)
        for (size_t i = 0; i < buffer_.size(); ++i)
        {
            if (delimiters_.contains(buffer_[i]))
            {
                // Packet is buffer_[0 .. i-1]
                out = etl::span<uint8_t>(buffer_.data(), i);

                // Remove consumed bytes (including delimiter at i)
                // Copy the payload into a static temp span before we modify buffer_
                etl::span<uint8_t> result(buffer_.data(), i);

                // Create a copy of that range into a small static buffer to keep it stable
                static uint8_t temp[256]; // adjust to your max expected packet size
                size_t len = (i < sizeof(temp)) ? i : sizeof(temp);
                etl::copy_n(buffer_.data(), len, temp);

                out = etl::span<uint8_t>(temp, len);

                // Now remove consumed bytes including the delimiter
                size_t remaining = buffer_.size() - (i + 1);
                if (remaining > 0)
                {
                    std::memmove(buffer_.data(),
                                 buffer_.data() + i + 1,
                                 remaining * sizeof(buffer_[0]));
                }
                buffer_.resize(remaining);
                return true;
            }
        }

        // Whne the buffer is fail, but no delimters just clear that buffer, and find the next delimter and
        // set the poinetr to that
        if (buffer_.full())
        {
            buffer_.clear();
            while (refPos_ < currentRef_.size())
            {
                if (delimiters_.contains(currentRef_[refPos_]))
                {
                    return pop_external(out);
                }
                refPos_ += 1;
            }
            buffer_tail(0);
            return false;
        }

        // No delimiter in buffer_ — append from currentRef_
        while (refPos_ < currentRef_.size())
        {
            uint8_t c = currentRef_[refPos_++];
            if (delimiters_.contains(c))
            {
                // delimiter found → buffer_ (which may be non-empty) is a complete packet
                out = etl::span<uint8_t>(buffer_.data(), buffer_.size());
                buffer_.clear();
                return true;
            }

            if (buffer_.full())
            {
                // cannot accept more bytes, caller must pop to free space
                return false;
            }
            buffer_.push_back(c);
        }

        // no delimiter yet
        return false;
    }

    // 2) Scan the external currentRef_ for a packet; if none found, copy tail into buffer_
    bool pop_external(etl::span<uint8_t> &out)
    {
        size_t start = refPos_;
        while (refPos_ < currentRef_.size())
        {
            if (delimiters_.contains(currentRef_[refPos_]))
            {
                out = currentRef_.subspan(start, refPos_ - start);
                ++refPos_;
                return true;
            }
            ++refPos_;
        }

        // no delimiter in external ref — copy remainder into internal buffer
        return buffer_tail(start);
    }

    // Copy remainder of currentRef_ (from start) into internal buffer (append).
    // Returns false (no complete packet produced).
    bool buffer_tail(size_t start)
    {
        if (start >= currentRef_.size())
            return false;

        size_t remaining = currentRef_.size() - start;
        size_t available = buffer_.max_size() - buffer_.size();
        size_t toCopy = (remaining < available) ? remaining : available;

        if (toCopy > 0)
        {
            buffer_.insert(buffer_.end(),
                           currentRef_.begin() + start,
                           currentRef_.begin() + start + toCopy);
        }
        return false;
    }

private:
    etl::ivector<uint8_t> &buffer_;
    const DelimiterBitmap &delimiters_;

    etl::span<uint8_t> currentRef_{};
    size_t refPos_{0};
};
