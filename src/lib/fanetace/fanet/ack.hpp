#pragma once

#include <stdint.h>
#include "header.hpp"

namespace FANET
{

    class AckPayload final
    {
    public:
    Header::MessageType type() const
        {
            return Header::MessageType::ACK;
        }

    };
}