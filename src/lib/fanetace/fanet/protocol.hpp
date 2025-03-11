#pragma once

#include "fanet.hpp"
#include "etl/optional.h"
#include "etl/vector.h"
#include "etl/bit_stream.h"
#include "etl/variant.h"
#include "etl/random.h"

#include "header.hpp"
#include "address.hpp"
#include "txFrame.hpp"
#include "tracking.hpp"
#include "name.hpp"
#include "message.hpp"
#include "utils.hpp"

#include "groundTracking.hpp"
#include "blockAllocator.hpp"
#include "packetParser.hpp"
#include "neighbourTable.hpp"

namespace FANET
{
    /**
     * @brief Protocol class for handling FANET communication.
     *
     * This class manages the sending and receiving of FANET packets, including handling
     * acknowledgments, forwarding, and maintaining a neighbor table.
     */
    class Protocol
    {
    private:
        static constexpr int32_t MAC_SLOT_MS = 20;

        static constexpr int32_t MAC_TX_MINPREAMBLEHEADERTIME_MS = 15;
        static constexpr int32_t MAC_TX_TIMEPERBYTE_MS = 2;
        static constexpr int32_t MAC_TX_ACKTIMEOUT = 1'000;
        static constexpr int32_t MAC_TX_RETRANSMISSION_TIME = 1'000;
        static constexpr int32_t MAC_TX_RETRANSMISSION_RETRYS = 3;
        static constexpr int32_t MAC_TX_BACKOFF_EXP_MIN = 7;
        static constexpr int32_t MAC_TX_BACKOFF_EXP_MAX = 12;

        static constexpr int16_t MAC_FORWARD_MAX_RSSI_DBM = -90; // todo test
        static constexpr int32_t MAC_FORWARD_MIN_DB_BOOST = 20;
        static constexpr int32_t MAC_FORWARD_DELAY_MIN = 100;
        static constexpr int32_t MAC_FORWARD_DELAY_MAX = 300;
        static constexpr int32_t FANET_MAX_NEIGHBORS = 30;

        static constexpr int32_t APP_VALID_STATE_MS = 10'000;

        static constexpr int32_t APP_TYPE1OR7_AIRTIME_MS = 40; // actually 20-30ms
        static constexpr int32_t APP_TYPE1OR7_MINTAU_MS = 250;
        static constexpr int32_t APP_TYPE1OR7_TAU_MS = 5'000;

        static constexpr int32_t FANET_CSMA_MIN = 20;
        static constexpr int32_t FANET_CSMA_MAX = 40;

        static constexpr int32_t MAC_MAXNEIGHBORS_4_TRACKING_2HOP = 5;
        static constexpr int32_t MAC_CODING48_THRESHOLD = 8;

        // Random number generator for random times
        etl::random_xorshift random; // XOR-Shift PRNG from ETL

        // Pool for all frames that need to be sent
        using TxPool = BlockAllocator<FANET::TxFrame, 50, 16>;
        TxPool txPool;

        // Table with received neighbors
        NeighbourTable<100> neighborTable_;

        // User's own address
        Address ownAddress_{1}; // Default to 1 to ensure 'ownAddress_' is not broadcast
        // When set to true, the protocol handler will forward received packages when applicable
        bool doForward = true;

        // When carrierReceivedTime has been set, then this is taken into consideration when to send the next packet
        // This is like CSMA in the old protocol.
        uint32_t carrierReceivedTime = 0;
        uint8_t carrierBackoffExp = 0;
        float airTime = 0;
        // Last time TX was done
        uint32_t lastTx = 0;
        // Connector for the application, e.g., the interface between the FANET protocol and the application
        Connector &connector;

    private:
        /**
         * @brief Build an acknowledgment packet from an existing packet.
         * @tparam MESSAGESIZE The size of the message payload.
         * @tparam NAMESIZE The size of the name payload.
         * @param packet The packet to acknowledge.
         * @return The acknowledgment packet.
         */
        template <size_t MESSAGESIZE, size_t NAMESIZE>
        auto buildAck(const Packet<MESSAGESIZE, NAMESIZE> &packet)
        {
            // fmac.260
            Packet<1, 1> ack;
            ack.source(ownAddress_).destination(packet.source());

            // fmac.265
            /* only do a 2 hop ACK in case it was requested and we received it via a two hop link (= forward bit is not set anymore) */
            if (packet.extendedHeader().value_or(ExtendedHeader{}).ack() == ExtendedHeader::AckType::TWOHOP &&
                !packet.header().forward())
            {
                ack.forward();
            }

            return ack.buildAck();
        }

        bool broadcastOrOwn(const Address &address) {
            return address == Address{} || address == ownAddress_ || address == Address{0xFF, 0xFFFF};
        }

    public:
        /**
         * @brief Constructor for the Protocol class.
         * @param connector_ The connector interface for the application.
         */
        Protocol(Connector &connector_) : connector(connector_)
        {
        }

        void ownAddress(const Address &adress)
        {
            // 0x and 0xFFFFFF are reserved
            if (adress == Address{0x00, 0x0000} || adress == Address{0xFF, 0xFFFF})
            {
                return;
            }
            ownAddress_ = adress;
        }

        const TxPool &pool() const
        {
            return txPool;
        }

        const NeighbourTable<100> &neighborTable() const {
            return neighborTable_;
        }

        /**
         * @brief Initialize the protocol.
         */
        void init()
        {
            random.initialise(connector.getTick());
        }

        /**
         * @brief Send a FANET packet.
         * @tparam MESSAGESIZE The size of the message payload.
         * @tparam NAMESIZE The size of the name payload.
         * @param packet The packet to send.
         * @param id ID of this packet, can be used if you request an ack and to know if the packet was received
         */
        template <size_t MESSAGESIZE, size_t NAMESIZE>
        void sendPacket(Packet<MESSAGESIZE, NAMESIZE> &packet, uint16_t id = 0)
        {
            auto v = packet.source(ownAddress_).build();
            auto txFrame = TxFrame{{v.data(), v.size()}}.id(id);
            txPool.add(txFrame);
        }

        /**
         * @brief Handle a received FANET packet.
         * @tparam MESSAGESIZE The size of the message payload.
         * @tparam NAMESIZE The size of the name payload.
         * @param rssddBm The received signal strength in dBm.
         * @param buffer The byte buffer containing the packet data.
         * @return The parsed FANET packet.
         */
        template <size_t MESSAGESIZE, size_t NAMESIZE>
        Packet<MESSAGESIZE, NAMESIZE> handleRx(int16_t rssddBm, etl::ivector<uint8_t> &buffer)
        {
            // START: OK
            static constexpr size_t MAC_PACKET_SIZE = ((MESSAGESIZE > NAMESIZE) ? MESSAGESIZE : NAMESIZE) + 12; // 12 Byte for maximum header size
            auto timeMs = connector.getTick();

            auto packet = PacketParser<MESSAGESIZE, NAMESIZE>::parse(buffer);
            packet.print();

            auto destination = packet.destination().value_or(Address{});
            auto extendedHeader = packet.extendedHeader().value_or(ExtendedHeader{});

            // fmac.283 Perhaps move this to some maintenance task?
            neighborTable_.removeOutdated(timeMs);

            // Drop reserved and own package. Own package might be a forwarded package
            if (packet.source() == ownAddress_)
            {
                return packet;
            }

            // fmac.322
            // addOrUpdate will guarantee this one is added, and any old one is removed
            neighborTable_.addOrUpdate(packet.source(), timeMs);

            // fmac.326
            // Decide if we have seen this frame already in the past, if so decide what to do with the frame in our buffer
            // This concerns forwarding of packets from other FANET devices
            auto frmList = frameInTxPool(buffer);
            if (frmList)
            {
                /* frame already in tx queue */
                if (rssddBm > (frmList->rssi() + MAC_FORWARD_MIN_DB_BOOST))
                {
                    /* received frame is at least 20dB better than the original -> no need to rebroadcast */
                    txPool.remove(frmList);
                }
                else
                {
                    /* adjusting new departure time */
                    // fmac.346
                    frmList->nextTx(connector.getTick() + random.range(MAC_FORWARD_DELAY_MIN, MAC_FORWARD_DELAY_MAX));
                }
            }
            // END: OK
            else
            {
                // When the package was destined to us or broadcast
                // fmac.351
                if (broadcastOrOwn(destination))
                {
                    // fmac.353
                    // When we receive an ack in broadcast or to us, but the frame was not found in our pool
                    if (packet.header().type() == Header::MessageType::ACK)
                    {
                        // fmac.cpp 356
                        removeDeleteAckedFrame(packet.source());
                    }
                    else
                    {
                        // fmac.361
                        // When ACK was requested
                        if (extendedHeader.ack() != ExtendedHeader::AckType::NONE)
                        {
                            // fmac.362
                            auto v = buildAck(packet);
                            // TODO: Check if destination is already IN the packet
                            txPool.add(TxFrame{/*destination,*/ v});
                        }

                        // fmac.366
                        // TODO: Inform app
                    }
                }

                // fmac.371
                if (doForward && packet.header().forward() &&
                    rssddBm <= MAC_FORWARD_MAX_RSSI_DBM &&
                    (broadcastOrOwn(destination) || neighborTable_.lastSeen(destination) &&
                                                     calculate3MinAirTime(airTime) < 0.5f))
                {

                    /* generate new tx time */
                    auto nextTx = timeMs + random.range(MAC_FORWARD_DELAY_MIN, MAC_FORWARD_DELAY_MAX);
                    auto numTx = extendedHeader.ack() == ExtendedHeader::AckType::NONE ? 0 : 1;

                    /* add to list */
                    // fmac.368
                    auto txFrame = TxFrame{/*destination,*/ etl::span<uint8_t>(buffer.data(), buffer.size())}
                                       .rssi(rssddBm)
                                       .numTx(numTx)
                                       .nextTx(nextTx)
                                       .forward(false);
                    txPool.add(txFrame);
                }
            }

            return packet;
        }

        /**
         * @brief Handle any packets in the queue.
         *
         * This can be called at regular intervals, or it can be called based on the time returned by this function when it
         * thinks a packet can be sent. This function might not always send a packet even if there are packets in the queue.
         *
         * @return The time in milliseconds until the next packet can be sent.
         */
        uint32_t handletx()
        {
            auto timeMs = connector.getTick();

            if (carrierReceivedTime < timeMs)
            {
                return carrierReceivedTime;
            }
            else
            {
                carrierReceivedTime = 0;
                carrierBackoffExp = 0;
            }

            // Get TxFrame if available
            // fmac.417
            auto frm = getNextTxFrame(timeMs);
            if (frm == nullptr || !connector.isBroadcastReady())
            {
                return timeMs + 500;
            }

            // fmac.428
            // Apparently FANET wants to send APP packets without consideration of airtime
            if (calculate3MinAirTime(airTime) > 0.9f && !frm->isPriorityPacket())
            {
                return 100;
            }

            // fmac.421
            if (neighborTable_.size() < MAC_MAXNEIGHBORS_4_TRACKING_2HOP)
            {
                frm->forward(true);
            }
            else
            {
                frm->forward(false);
            }

            // fmac.446
            if (frm->ackType() != ExtendedHeader::AckType::NONE && frm->numTx() < 0)
            {
                // Why inform the app??
                txPool.remove(frm);
                return 0;
            }

            // fmac.457
            auto destination = frm->destination();
            if (frm->forward() &&
                frm->destination() != Address{} &&
                neighborTable_.lastSeen(destination) == 0)
            {
                frm->forward(true);
            }

            // Send data
            // fmac.502
            auto frameLength = frm->data().end() - frm->data().begin();
            auto cr = neighborTable_.size() < MAC_CODING48_THRESHOLD ? 8 : 5;
            connector.sendFrame(cr, frm->data());
            lastTx = connector.getTick();

            airTime += FANET::calculateAirtime(frameLength, 7, 250, cr - 4);

            // fmac.522
            if (frm->ackType() != ExtendedHeader::AckType::NONE || frm->source() != ownAddress_)
            {
                // fmac.524
                txPool.remove(frm);
            }
            else if (frm->numTx() > 0)
            {
                // fmac.531
                frm->numTx(frm->numTx() - 1);
                frm->nextTx(connector.getTick() + (MAC_TX_RETRANSMISSION_TIME * (MAC_TX_RETRANSMISSION_RETRYS - frm->numTx())));
            }
            else
            {
                // fmac.53
                frm->nextTx(connector.getTick() + MAC_TX_ACKTIMEOUT);
            }

            auto nextUp = getNextTxFrame(timeMs);
            if (nextUp)
            {
                return connector.getTick() + MAC_TX_MINPREAMBLEHEADERTIME_MS + (frameLength * MAC_TX_TIMEPERBYTE_MS);
            }
            else
            {
                return connector.getTick() + 500;
            }
        }

        /**
         * @brief Call this when your receiver detects a carrier.
         */
        void carrierDetected()
        {
            if (carrierReceivedTime == 0)
            {
                carrierReceivedTime = connector.getTick() + MAC_TX_MINPREAMBLEHEADERTIME_MS + (255 * MAC_TX_TIMEPERBYTE_MS);
            }
            else
            {
                if (carrierBackoffExp < MAC_TX_BACKOFF_EXP_MAX)
                {
                    carrierBackoffExp++;
                }
                carrierReceivedTime = connector.getTick() + random.range(1 << (MAC_TX_BACKOFF_EXP_MIN - 1), 1 << carrierBackoffExp);
            }
        }

    private:
        /**
         * @brief Calculate the 3-minute average airtime.
         * @param airTime The current airtime.
         * @return The 3-minute average airtime.
         */
        float calculate3MinAirTime(float airTime)
        {
            static uint32_t last = 0;
            uint32_t current = connector.getTick();
            uint32_t dt = current - last;
            last = current;

            /* reduce airtime by 1% */
            airTime -= dt * 10.f;
            if (airTime < 0.0f)
            {
                airTime = 0.0f;
            }

            /* air time over 3min average -> 1800ms air time allowed in 1% terms */
            return airTime / 1.8f;
        }

        /**
         * @brief Get the next TX block that can be transmitted.
         *
         * This function finds the next TX block that can be transmitted in the following order:
         * a) Find any tracking packet.
         * b) Find any acknowledgment packet.
         * c) Find any other packet.
         *
         * @param timeMs The current time in milliseconds.
         * @return A pointer to the next TX frame, or nullptr if no frame is available.
         */
        TxFrame *getNextTxFrame(uint32_t timeMs)
        {
            // Find Tracking packet
            auto trackIt = std::min_element(txPool.begin(), txPool.end(), [timeMs](auto &a, auto &b)
                                            {
        
                                              if (a.isPriorityPacket() && b.isPriorityPacket()) 
                                                  return a.nextTx() < b.nextTx();
                                              if (a.isPriorityPacket()) 
                                                  return true;
                                              return false; });

            if (trackIt != txPool.end() &&
                trackIt->isPriorityPacket() &&
                trackIt->nextTx() <= timeMs)
            {
                return &(*trackIt);
            }

            // Find the ACK packet with the earliest nextTx() time
            auto ackIt = std::min_element(txPool.begin(), txPool.end(), [timeMs](auto &a, auto &b)
                                          {
                                              bool aIsAck = a.type() == Header::MessageType::ACK;
                                              bool bIsAck = b.type() == Header::MessageType::ACK;
        
                                              if (aIsAck && bIsAck) 
                                                  return a.nextTx() < b.nextTx();
                                              if (aIsAck) 
                                                  return true;
                                              return false; });

            if (ackIt != txPool.end() && ackIt->type() == Header::MessageType::ACK && ackIt->nextTx() <= timeMs)
            {
                return &(*ackIt);
            }

            // Otherwise, find the packet with the earliest nextTx() time
            auto it = std::min_element(txPool.begin(), txPool.end(), [](const auto &a, const auto &b)
                                       { return a.nextTx() < b.nextTx(); });

            if (it != txPool.end() && it->nextTx() <= timeMs)
            {
                return &(*it);
            }

            return nullptr;
        }

        /**
         * @brief Find a frame in the TX pool that matches the given buffer.
         * @param buffer The buffer to match.
         * @return A pointer to the matching TX frame, or nullptr if no match is found.
         */
        TxFrame *frameInTxPool(const etl::span<uint8_t> buffer)
        {
            auto it = etl::find_if(txPool.begin(), txPool.end(),
                                   [&buffer](auto block)
                                   {
                                       // frame.162
                                       auto other = TxFrame{buffer};

                                       if (block.source() != other.source())
                                       {
                                           return false;
                                       }
                                       if (block.data().size() != buffer.size())
                                       {
                                           return false;
                                       }
                                       if (block.destination() != other.destination())
                                       {
                                           return false;
                                       }
                                       if (block.type() != other.type())
                                       {
                                           return false;
                                       }

                                       if (!etl::equal(block.payload(), other.payload()))
                                       {
                                           return false;
                                       }

                                       return true;
                                   });

            if (it != txPool.end())
            {
                return &(*it);
            }

            return nullptr;
        }

        /**
         * @brief Remove any pending frame that waits on an ACK from a host
         * @param destination The destination address of the acknowledged frames.
         */
        void removeDeleteAckedFrame(const Address &destination)
        {
            for (auto it = txPool.begin(); it != txPool.end();)
            {
                if (it->destination() == destination && it->ackType() != ExtendedHeader::AckType::NONE)
                {
                    it = txPool.remove(it);
                }
                else
                {
                    ++it;
                }
            }
        }
    };

};
