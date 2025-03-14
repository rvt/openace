#pragma once

#include "ack.hpp"
#include "tracking.hpp"
#include "name.hpp"
#include "message.hpp"
#include "groundTracking.hpp"
#include "address.hpp"
#include "header.hpp"
#include "packet.hpp"
#include "extendedHeader.hpp"


namespace FANET {

    class Connector {

        public:

        virtual ~Connector() { }


        /**
         * @brief return a number of ms. Does not matter if it's from epoch, as long as it's monotonic.
         */
        virtual uint32_t getTick() const = 0;

        virtual bool sendFrame(uint8_t codingRate, const etl::span<uint8_t> &data) = 0;

        /* device -> air */
        // virtual bool is_broadcast_ready(int num_neighbors) = 0;
        // virtual void broadcast_successful(int type) = 0;
        // virtual Frame *get_frame() = 0;
    
        // /* air -> device */
        virtual void ackReceived(uint16_t id) = 0;
        // virtual void nacked(bool id, const Address &addr) = 0;
        // virtual void handle_frame(Frame *frm) = 0;

        
        // virtual void acked(uint16_t id, const Address &addr) = 0;
        // virtual void nacked(bool id, const Address &addr) = 0;

    };
    
}
//#include "packetBuilder.hpp"
//#include "packetParser.hpp"
