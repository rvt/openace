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
#include "groundTracking.hpp"
#include "blockAllocator.hpp"
#include "packetParser.hpp"
#include "neighbourTable.hpp"

namespace FANET
{
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

        // Pool for all frames that need to be send
        using TxPool = BlockAllocator<FANET::TxFrame, 50, 16>;
        TxPool txPool;

        // Table with received naugbours
        NeighbourTable<100> neighborTable;

        // Users own address
        Address ownAddress;
        // When set to true, the protocol handler will forward received packages when applicable
        bool doForward = true;

        // When carrierReceivedTime has been set, then this is taken into consideration when to send the next packet
        // THis is line CSMA in the old protocol.
        uint32_t carrierReceivedTime = 0;
        uint8_t carrierBackoffExp = 0;
        float airTime = 0;
        // Last time TX was done
        uint32_t lastTx = 0;
        // Connector for the applciation, eg the interface between the FANET protocol and the application
        Connector &connector;

    private:
        /**
         * Build an ack package from existing packet
         */
        template <size_t MESSAGESIZE, size_t NAMESIZE>
        auto buildAck(const Packet<MESSAGESIZE, NAMESIZE> &packet)
        {
            // fmac.260
            Packet<1, 1> ack;
            ack.source(ownAddress);
            ack.destination(packet.source());

            // fmac.265
            /* only do a 2 hop ACK in case it was requested and we received it via a two hop link (= forward bit is not set anymore) */
            if (packet.extendedHeader().value_or(ExtendedHeader{}).ack() == ExtendedHeader::AckType::TWOHOP &&
                !packet.header().forward())
            {
                ack.forward();
            }

            return ack.buildAck();
        }

    public:
        Protocol(Connector &connector_) : connector(connector_)
        {
        }

        void init()
        {
            random.initialise(connector.getTick());
        }

        template <size_t MESSAGESIZE, size_t NAMESIZE>
        void sendPacket(const Packet<MESSAGESIZE, NAMESIZE> &packet)
        {
            auto v = packet.build();
            auto txFrame = TxFrame{Address{}, {v.data(), v.size()}};
            txPool.allocate(txFrame);
        }

        template <size_t MESSAGESIZE, size_t NAMESIZE>
        Packet<MESSAGESIZE, NAMESIZE> handleRx(int16_t rssddBm, etl::ivector<uint8_t> &buffer)
        {
            static constexpr size_t MAC_PACKET_SIZE = ((MESSAGESIZE > NAMESIZE) ? MESSAGESIZE : NAMESIZE) + 12; // 12 Byte for maximum header size
            auto timeMs = connector.getTick();

            auto packet = PacketParser<MESSAGESIZE, NAMESIZE>::parse(buffer);
            auto destination = packet.destination().value_or(Address{0});
            auto extendedHeader = packet.extendedHeader().value_or(ExtendedHeader{});

            // fmac.283 Perhaps move this to some maintenance task?
            neighborTable.removeOutdated(timeMs);
            // fmac.322
            // addOrUpdate will guarantiee we can update this one
            neighborTable.addOrUpdate(packet.source(), timeMs);

            // Drop reserved and own package. Own package might be a forwarded package
            if (packet.source() == Address{0} ||
                packet.source() == Address{0xFFFFFF} ||
                packet.source() == ownAddress)
            {
                return packet;
            }

            // fmac.326
            auto frmList = frameInTxPool(buffer);
            if (frmList)
            {
                if (rssddBm > (frmList->rssi() + MAC_FORWARD_MIN_DB_BOOST))
                {
                    /* received frame is at least 20dB better than the original -> no need to rebroadcast */
                    txPool.deallocate(*frmList);
                }
                else
                {
                    /* adjusting new departure time */
                    frmList->nextTx(connector.getTick() + random.range(MAC_FORWARD_DELAY_MIN, MAC_FORWARD_DELAY_MAX));
                }
            }
            else
            {
                // When the package was destined to us or 0
                // fmac.351
                if (destination == Address{} || destination == ownAddress)
                {
                    // fmac.353
                    if (packet.header().type() == Header::MessageType::ACK)
                    {
                        // fmac.cpp 356
                        // When we send a packet directly a destination and when we requested an ack, this will remove the
                        removeDeleteAckedFrame(packet.source());
                        // TODO: Inform app
                    }
                    else
                    {
                        // fmac.361
                        if (extendedHeader.ack() != ExtendedHeader::AckType::NONE)
                        {
                            // fmac.362
                            auto v = buildAck(packet);
                            // TODO: Check if destination is already IN the packet
                            txPool.allocate(TxFrame{/*destination,*/ v});
                        }

                        // fmac.366
                        // TODO: Inform app
                    }
                }

                // fmac.371
                if (doForward && packet.header().forward() &&
                    rssddBm <= MAC_FORWARD_MAX_RSSI_DBM &&
                    (destination == Address{0} || neighborTable.lastSeen(destination) &&
                                                      calculate3MinAirTime(airTime) < 0.5f))
                {

                    /* generate new tx time */
                    auto nextTx = timeMs + random.range(MAC_FORWARD_DELAY_MIN, MAC_FORWARD_DELAY_MAX);
                    auto numTx = extendedHeader.ack() == ExtendedHeader::AckType::NONE ? 0 : 1;

                    /* add to list */
                    // fmac.368
                    auto txFrame = TxFrame{/*destination,*/ {buffer.data(), buffer.size()}}.rssi(rssddBm).numTx(numTx).nextTx(nextTx);
                    txFrame.forward(false);
                    txPool.allocate(txFrame);
                }
            }

            return packet;
        }

        /**
         * @Brief Calling this method will handle any opackages in the queue
         * THis can be called at regular intervals, or it can be called based on the time returned by this function when it
         * thinks a packet can be send. THis function might not always send a packet even if there are packages in the queue
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
            // Apparently FANEt want's to send APP packets without consideration of airtime
            if (calculate3MinAirTime(airTime) > 0.9f && !frm->isPriorityPacket())
            {
                return 100;
            }

            // fmac.421
            if (neighborTable.size() < MAC_MAXNEIGHBORS_4_TRACKING_2HOP)
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
                txPool.deallocate(*frm);
                return 0;
            }

            // fmac.457
            auto destination = frm->destination();
            if (frm->forward() &&
                frm->destination() != Address{} &&
                neighborTable.lastSeen(destination) == 0)
            {
                frm->forward(true);
            }

            // Send data
            // fmac.502
            auto frameLength = frm->data().end() - frm->data().begin();
            auto cr = neighborTable.size() < MAC_CODING48_THRESHOLD ? 8 : 5;
            connector.sendFrame(cr, frm->data());
            lastTx = connector.getTick();

            airTime += calculateAirtime(frameLength, 7, 250, cr - 4);

            // fmac.522
            if (frm->ackType() != ExtendedHeader::AckType::NONE || frm->source() != ownAddress)
            {
                // fmac.524
                txPool.deallocate(*frm);
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
         * @brief call this when your received detects a carrier
         */
        void carrierDetected()
        {
            if (carrierReceivedTime == 0)
            {
                carrierReceivedTime =  connector.getTick() + MAC_TX_MINPREAMBLEHEADERTIME_MS + (255 * MAC_TX_TIMEPERBYTE_MS);
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
         * Get the next TX block that can be transmitted in this other
         * a) FInd ant tracking packet
         * b) FInd any ack packet
         * c) Find any other packet
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

        // fmac.140
        void removeDeleteAckedFrame(const Address &destination)
        {
            for (auto it = txPool.begin(); it != txPool.end();)
            {
                if (it->destination() == destination && it->ackType() != ExtendedHeader::AckType::NONE)
                {
                    auto toErase = it;
                    ++it;
                    txPool.deallocate(*toErase);
                }
                else
                {
                    ++it;
                }
            }
        }

        float calculateAirtime(int size,
                               int sf,
                               int bw,
                               int cr = 1, // Coding Rate in 1:4/5 2:4/6 3:4/7 4:4/8
                               int lowDrOptimize = 2 /*2:auto 1:yes 0:no*/,
                               bool explicitHeader = true,
                               int preambleLength = 8)
        {
            // Symbol time in milliseconds
            float tSym = (std::pow(2, sf) / (bw * 1000)) * 1000.0;

            // Preamble time
            float tPreamble = (preambleLength + 4.25) * tSym;

            // Header: 0 when explicit, 1 when implicit
            int h = explicitHeader ? 0 : 1;

            // Low Data Rate Optimization (DE)
            int de = ((lowDrOptimize == 2 && bw == 125 && sf >= 11) || lowDrOptimize == 1) ? 1 : 0;

            // Extract coding rate from "4/x" format (e.g., "4/5" -> cr = 1)
            // int cr = std::stoi(codingRate.substr(2)) - 4;

            // Calculate number of payload symbols
            int payloadSymbNb = 8 + etl::max(
                                        (int)std::ceil((8 * size - 4 * sf + 28 + 16 - 20 * h) / (4.0 * (sf - 2 * de))) * (cr + 4),
                                        0);

            // Payload time
            float tPayload = payloadSymbNb * tSym;

            // Total airtime in milliseconds
            return tPreamble + tPayload;
        }
    };

};
