#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <optional>
#include <cstring>

#include "fanet_common.h"

namespace FANET
{

    // FANET MAC Structure
    struct MACFrame
    {
        Header header;
        Address source;
        ExtendedHeader extHeader;     // Present if header.extended is set
        Address destination;          // Present if extHeader->cast is set
        uint32_t signature;           // Present if extHeader->signature is set
        std::vector<uint8_t> payload; // Message payload

        static MACFrame fromBytes(const std::vector<uint8_t>& data) {
            MACFrame frame;
            size_t index = 0;
    
            // Parse Header
            frame.header = *reinterpret_cast<const Header*>(&data[index]);
            index += sizeof(Header);
    
            // Parse Source Address
            frame.source = *reinterpret_cast<const Address*>(&data[index]);
            index += sizeof(Address);
    
            // Parse Extended Header if present
            if (frame.header.extended) {
                frame.extHeader = *reinterpret_cast<const ExtendedHeader*>(&data[index]);
                index += sizeof(ExtendedHeader);
                
                // Parse Destination Address if Cast is set
                if (frame.extHeader->cast) {
                    frame.destination = *reinterpret_cast<const Address*>(&data[index]);
                    index += sizeof(Address);
                }
    
                // Parse Signature if Signature bit is set
                if (frame.extHeader->signature) {
                    frame.signature = *reinterpret_cast<const uint32_t*>(&data[index]);
                    index += sizeof(uint32_t);
                }
            }
    
            // Remaining data is payload
            frame.payload.assign(data.begin() + index, data.end());
            return frame;
        }
        

    };


} 
