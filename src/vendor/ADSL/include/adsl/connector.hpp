#pragma once

#include "connector.hpp"
#include "etl/span.h"

namespace ADSL
{

    class TrafficPayload;
    class StatusPayload;
    class Header;
    class Status;
    class Connector
    {

    public:
        virtual ~Connector() {}

        /**
         * @brief return a number of ms. Does not matter if it's from epoch, as long as it's monotonic.
         */
        virtual uint32_t adsl_getTick() const = 0;

        /**
         * @brief send a frame to the network
         * @param codingRate the coding rate to use
         * @param data the data to send
         * @return true if the frame was sent successfully
         */
        virtual bool adsl_sendFrame(const void * ctx, etl::span<const uint8_t> data) = 0;

        // Called when a payload/header/status is received by the protocol layer
        virtual void adsl_receivedTraffic(const Header &header, const ADSL::TrafficPayload &tp) = 0;
        virtual void adsl_receivedStatus(const ADSL::Header &header, const ADSL::StatusPayload &sp) = 0;

        /**
         * Request to build a trffic payload
         * Must be implemented to send any Traffic
         */
        virtual void adsl_buildTraffic(const void *ctx, ADSL::TrafficPayload &tp) = 0;

        /**
         * Request to build a status payload
         * It'a advised to override this in fill in your capabilities
         */
        virtual void adsl_buildStatusPayload(const void *ctx, ADSL::StatusPayload &sp) = 0;
    };

}
