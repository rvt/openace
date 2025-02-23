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
    };
}