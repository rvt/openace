#pragma once

#include <stdint.h>
#include "etl/string.h"
#include "header.hpp"

namespace FANET
{
    /**
     * Name Payload
     * Messagetype : 2
     */
    template <size_t SIZE>
    class NamePayload final
    {
        etl::string<SIZE> nameRaw;
        static_assert(SIZE <= 245, "NamePayload size cannot exceed 245 bytes");

    public:
        NamePayload () = default;
        Header::MessageType type() const
        {
            return Header::MessageType::NAME;
        }

        etl::string_view name() const
        {
            return etl::string_view(nameRaw);
        }

        void name(const etl::string_view &name)
        {
            nameRaw.assign(name.data(), name.size());
        }

        virtual void serialize(etl::bit_stream_writer &writer) const
        {
            for (auto value : nameRaw)
            {
                writer.write_unchecked<uint8_t>(value);
            }
        }

        static const NamePayload deserialize(etl::bit_stream_reader &reader)
        {
            NamePayload payload;
            auto available_bytes = reader.size_bytes();
            if (available_bytes < 1) {
                return payload;
            }

            auto copy_size = std::min(available_bytes, SIZE);
            payload.nameRaw.assign((uint8_t *)(reader.cbegin()), (uint8_t *)(reader.cbegin() + copy_size) );

            return payload;
        }
    };
}