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
         * Should return a monotonic increating time in ms it's not required to be aligned with PPS or epoch.
         * This is used to calculate frame timing
         */
        virtual uint32_t timeMs() const = 0;

        /* device -> air */
        // virtual bool is_broadcast_ready(int num_neighbors) = 0;
        // virtual void broadcast_successful(int type) = 0;
        // virtual Frame *get_frame() = 0;
    
        // /* air -> device */
        // virtual void handle_acked(bool ack, MacAddr &addr) = 0;
        // virtual void handle_frame(Frame *frm) = 0;
    
    };
    
}
//#include "packetBuilder.hpp"
//#include "packetParser.hpp"
