#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <optional>
#include <cstring>

namespace FANET
{

    // FANET Header Structure
    struct Header
    {
        uint8_t type : 6;     // Type of message (6 bits)
        uint8_t forward : 1;  // Forwarding bit
        uint8_t extended : 1; // Extended header bit
    };

    // FANET Address Structure
    struct Address
    {
        uint8_t manufacturerID; // Manufacturer ID
        uint16_t uniqueID;      // Unique Device ID
    };

    // FANET Extended Header
    struct ExtendedHeader
    {
        uint8_t ack : 2;       // Acknowledgment
        uint8_t cast : 1;      // Unicast or Broadcast
        uint8_t signature : 1; // Signature presence
        uint8_t reserved : 4;  // Reserved bits
    };

    // FANET Tracking Structure
    struct TrackingType1
    {
        int32_t latitude;       // Scaled by 93206
        int32_t longitude;      // Scaled by 46603
        uint16_t altitude : 11; // 11-bit altitude
        uint8_t aScaling : 1;
        uint8_t aircraftType : 3;
        uint8_t tracking : 1;
        uint8_t speed : 7;
        uint8_t sScaling : 1;
        uint8_t climb;
        uint8_t cScaling : 1;
        uint8_t heading;
        uint8_t turnRate;
        uint8_t tScaling : 1;
        uint8_t reserved : 4; // Reserved bits to align struct
    };

    // FANET Name Message Structure
    struct NameType2
    {
        char name[245]; // UTF-8 encoded name, max length 245 bytes
    };

    // FANET Message Structure for Type 3 (Text Message)
    struct MessageType3
    {
        char message[244]; // UTF-8 encoded message, max length 244 bytes
    };

    // FANET Service Structure for Type 4
    struct ServiceType4
    {
        uint8_t serviceData[12]; // Placeholder for service-related data
    };

    // FANET Ground Tracking Structure for Type 7
    struct GroundTrackingType7
    {
        int32_t latitude;  // Scaled by 93206
        int32_t longitude; // Scaled by 46603
        uint8_t groundType;
        uint8_t tracking;
    };

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
        
        // Function to set Name Type 2
        void setNameType2(const std::string &name)
        {
            header.type = NAME;
            payload.clear();
            NameType2 nameData = {};
            strncpy(nameData.name, name.c_str(), sizeof(nameData.name) - 1);
            const uint8_t *ptr = reinterpret_cast<const uint8_t *>(&nameData);
            payload.insert(payload.end(), ptr, ptr + sizeof(NameType2));
        }

        // Function to set Message Type 3
        void setMessageType3(const std::string &message)
        {
            header.type = MESSAGE;
            payload.clear();
            MessageType3 messageData = {};
            strncpy(messageData.message, message.c_str(), sizeof(messageData.message) - 1);
            const uint8_t *ptr = reinterpret_cast<const uint8_t *>(&messageData);
            payload.insert(payload.end(), ptr, ptr + sizeof(MessageType3));
        }

        // Function to set Service Type 4
        void setServiceType4(const std::vector<uint8_t> &serviceData)
        {
            header.type = SERVICE;
            payload.clear();
            ServiceType4 serviceMessage = {};
            memcpy(serviceMessage.serviceData, serviceData.data(),
                   std::min(sizeof(serviceMessage.serviceData), serviceData.size()));
            const uint8_t *ptr = reinterpret_cast<const uint8_t *>(&serviceMessage);
            payload.insert(payload.end(), ptr, ptr + sizeof(ServiceType4));
        }

        // Function to set Ground Tracking Type 7
        void setGroundTrackingType7(int32_t latitude, int32_t longitude,
                                    uint8_t groundType, uint8_t tracking)
        {
            header.type = GROUND_TRACKING;
            payload.clear();
            GroundTrackingType7 trackingData = {latitude, longitude, groundType, tracking};
            const uint8_t *ptr = reinterpret_cast<const uint8_t *>(&trackingData);
            payload.insert(payload.end(), ptr, ptr + sizeof(GroundTrackingType7));
        }
    };

    // FANET Message Types
    enum MessageType : uint8_t
    {
        ACK = 0,
        TRACKING = 1,
        NAME = 2,
        MESSAGE = 3,
        SERVICE = 4,
        LANDMARKS = 5,
        REMOTE_CONFIG = 6,
        GROUND_TRACKING = 7
    };

} // namespace FANET

#endif // FANET_H
