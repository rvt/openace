#pragma once

#include "fanet.hpp"
#include "etl/optional.h"
#include "etl/vector.h"

namespace FANET
{
    class PacketParser final 
    {
    private:
        Header header{};
        Address source{};
        etl::optional<Address> destination;
        etl::optional<ExtendedHeader> extHeader;
        etl::optional<uint32_t> signature;

        PacketParser();
    public:



        PacketParser parse(const  RadioPacket &packet) {

        }
};
