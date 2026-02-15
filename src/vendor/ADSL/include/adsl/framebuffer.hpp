#pragma once

#include "etl/array.h"
#include "etl/span.h"
#include "etl/memory.h"

namespace ADSL
{

    template <size_t MaxWords>
    struct FrameBuffer
    {
        static_assert(MaxWords > 0, "Buffer must have storage");
        etl::array<uint32_t, MaxWords> storage{};
        size_t used_bytes = 0;

        // Construct from a span of 32-bit words (constexpr-capable)
        constexpr FrameBuffer(etl::span<const uint32_t> words)
            : storage(), used_bytes(0)
        {
            assign(words);
        }

        constexpr FrameBuffer() : storage(), used_bytes(0)
        {
        }
        //      assert(((reinterpret_cast<uintptr_t>(storage.data()) & 0x3) == 0) && "Data not 32-bit aligned");

        constexpr etl::span<uint32_t> fullSpan32()
        {
            return etl::span(storage.data(), MaxWords);
        }

        constexpr etl::span<uint32_t> usedSpan32()
        {
            return etl::span(storage.data(), (used_bytes + 3) / 4);
        }

        constexpr uint8_t &operator[](size_t index)
        {
            // Debug-only guard is a good idea on MCUs
            return fullSpan()[index];
        }

        constexpr const uint8_t &operator[](size_t index) const
        {
            return fullSpan()[index];
        }

        static constexpr size_t maxSizeBytes()
        {
            return MaxWords * sizeof(uint32_t);
        }

        constexpr etl::span<uint8_t> fullSpan()
        {
            return {
                reinterpret_cast<uint8_t *>(storage.data()), maxSizeBytes()};
        }

        constexpr etl::span<const uint8_t> fullSpan() const
        {
            return {
                reinterpret_cast<const uint8_t *>(storage.data()), maxSizeBytes()};
        }

        constexpr etl::span<const uint8_t> usedSpan() const
        {
            return {reinterpret_cast<const uint8_t *>(storage.data()), used_bytes};
        }

        constexpr etl::span<uint8_t> usedSpan()
        {
            return {reinterpret_cast<uint8_t *>(storage.data()), used_bytes};
        }

        // Explicit word access when needed
        constexpr uint32_t *words()
        {
            return storage.data();
        }

        constexpr const uint32_t *words() const
        {
            return storage.data();
        }

        // Assign a sequence of 32-bit words into the storage and update used_bytes
        void assign(etl::span<const uint32_t> words)
        {
            size_t copy_words = words.size();
            if (copy_words > MaxWords)
            {
                copy_words = MaxWords;
            }
            storage.assign(words.data(), words.data() + static_cast<size_t>(copy_words));
            used_bytes = copy_words * sizeof(uint32_t);
        }

        // Assign raw bytes into the storage's byte view. Excess bytes are ignored.
        void assign(etl::span<const uint8_t> bytes)
        {
            size_t byte_count = bytes.size();
            const size_t max_bytes = maxSizeBytes();
            if (byte_count > max_bytes)
            {
                byte_count = max_bytes;
            }

            auto u = fullSpan();
            for (size_t i = 0; i < byte_count; i++)
            {
                u[i] = bytes[i];
            }
            used_bytes = byte_count;
        }

        void reverseBits()
        {
            auto bspan = usedSpan();
            for (size_t i = 0; i < bspan.size(); i++)
            {
                bspan[i] = etl::reverse_bits(bspan[i]);
            }
        }

        /**
         * Shift one word to the right
         */
        void shiftOneWord()
        {
            auto bspan = fullSpan32();
            size_t words = (used_bytes + 3) / 4;
            etl::mem_move(&bspan[0], words, &bspan[1]);
            used_bytes += 4;
        }
    };

}
