#pragma once

#include <stdint.h>
#include "etl/string.h"
#include "header.hpp"

namespace FANET
{
    /**
     * @brief Name Payload for FANET protocol.
     * Messagetype : 2
     * @tparam SIZE The size of the name payload.
     */
    template <size_t SIZE>
    class NamePayload final
    {
        etl::string<SIZE> nameRaw = {}; // Raw name data
        static_assert(SIZE <= 245, "NamePayload size cannot exceed 245 bytes");

    public:
        /**
         * @brief Default constructor.
         */
        NamePayload() = default;

        /**
         * @brief Get the message type.
         * @return The message type.
         */
        Header::MessageType type() const
        {
            return Header::MessageType::NAME;
        }

        /**
         * @brief Get the name.
         * @return The name as a string view.
         */
        etl::string_view name() const
        {
            return etl::string_view(nameRaw);
        }

        /**
         * @brief Set the name.
         * @param name The name as a string view.
         */
        void name(const etl::string_view &name)
        {
            nameRaw.assign(name.data(), name.size());
        }

        /**
         * @brief Serialize the name payload to a bit stream.
         * @param writer The bit stream writer.
         */
        virtual void serialize(etl::bit_stream_writer &writer) const
        {
            for (auto value : nameRaw)
            {
                writer.write_unchecked<uint8_t>(value);
            }
        }

        /**
         * @brief Deserialize the name payload from a bit stream.
         * @param reader The bit stream reader.
         * @return The deserialized name payload.
         */
        static const NamePayload deserialize(etl::bit_stream_reader &reader)
        {
            NamePayload payload;
            while (payload.nameRaw.size() < SIZE)
            {
                auto byteOpt = reader.read<uint8_t>();
                if (byteOpt == etl::nullopt)
                {
                    break;
                }
                payload.nameRaw.push_back(static_cast<char>(*byteOpt));
            }
            return payload;
        }
    };
}