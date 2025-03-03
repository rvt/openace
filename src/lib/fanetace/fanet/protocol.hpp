#pragma once

#include "fanet.hpp"
#include "etl/optional.h"
#include "etl/vector.h"
#include "etl/bit_stream.h"
#include "etl/variant.h"
#include "etl/random.h"

#include "header.hpp"
#include "address.hpp"
#include "tracking.hpp"
#include "name.hpp"
#include "message.hpp"
#include "groundTracking.hpp"
#include "blockAllocator.hpp"
#include "packetParser.hpp"
#include "neighbourTable.hpp"

namespace FANET
{

    /**
     * Class that holds a raw Packet in binary form
     */
    class TxFrame
    {
        friend class Protocol;

    private:
        etl::span<uint8_t> block_;
        Address destination;
        uint8_t numTx_ = 0;
        uint32_t nextTx_ = 0;
        int16_t rssi_;

        TxFrame(Address destination_, const etl::span<uint8_t> &block) : destination(destination_), block_(block), numTx_(0), nextTx_(0) {}

        TxFrame &numTx(uint8_t v)
        {
            numTx_ = v;
            return *this;
        }
        TxFrame &rssi(int16_t v)
        {
            rssi_ = v;
            return *this;
        }
        TxFrame &nextTx(uint32_t v)
        {
            nextTx_ = v;
            return *this;
        }
        uint32_t nextTx() const
        {
            return nextTx_;
        }
        uint8_t numTx() const
        {
            return numTx_;
        }
        uint8_t rssi() const
        {
            return rssi_;
        }

    public:
        etl::span<uint8_t> data() const
        {
            return block_;
        }
        void data(etl::span<uint8_t> v)
        {
            block_ = v;
        }
    };

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

        // Random number generator for random times
        etl::random_xorshift random; // XOR-Shift PRNG from ETL

        // Pool for all frames that need to be send
        using TxPool = BlockAllocator<TxFrame, 50, 16>;
        TxPool txPool;

        // Table with received naugbours
        NeighbourTable<100> neighborTable;

        // Users own address
        Address ownAddress;
        // When set to true, the protocol handler will forward received packages when applicable
        bool doForward = true;

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
                ack.forward(true);
            }

            return ack.buildAck();
        }

    public:
        Protocol(Connector &connector_) : connector(connector_)
        {
        }

        void init()
        {
            random.initialise(connector.timeMs());
        }

        template <size_t MESSAGESIZE, size_t NAMESIZE>
        Packet<MESSAGESIZE, NAMESIZE> handleRx(int16_t rssddBm, const etl::ivector<uint8_t> &buffer)
        {
            static constexpr size_t MAC_PACKET_SIZE = ((MESSAGESIZE > NAMESIZE) ? MESSAGESIZE : NAMESIZE) + 12; // 12 Byte for maximum header size
            auto timeMs = connector.timeMs();

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
                    frmList->nextTx(connector.timeMs() + random.range(MAC_FORWARD_DELAY_MIN, MAC_FORWARD_DELAY_MAX));
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
                            txPool.allocate(TxFrame{destination, buildAck(packet)});
                        }

                        // fmac.366
                        // TODO: Inform app
                    }
                }

                // fmac.371
                if (doForward && packet.header().forward() &&
                    rssddBm <= MAC_FORWARD_MAX_RSSI_DBM &&
                    (destination == Address{0} || neighborTable.lastSeen(destination))
                    /* TODOL: get air limit */
                    )
                {

                    // BEGIN: Alternative to serialise the packet again, might be slower?
                    /* prevent from re-forwarding */
                    // packet.forward(false);
                    // auto newBuffer = packet.build(); // Check if we can prevent this.
                    // END Alternative

                    // BEGIN: Alternative to set the forward bit which might be faster, but more stack?
                    etl::vector<uint8_t, MAC_PACKET_SIZE> newBuffer;
                    std::copy(buffer.begin(), buffer.end(), std::back_inserter(newBuffer));
                    /* prevent from re-forwarding */
                    PacketRef{newBuffer}.forward(false);
                    // END Alternative

                    /* generate new tx time */
                    auto nextTx = timeMs + random.range(MAC_FORWARD_DELAY_MIN, MAC_FORWARD_DELAY_MAX);
                    auto numTx = extendedHeader.ack() == ExtendedHeader::AckType::NONE ? 0 : 1;

                    /* add to list */
                    // fmac.368
                    txPool.allocate(TxFrame{destination, etl::span<uint8_t>(newBuffer.data(), newBuffer.size())}.rssi(rssddBm).numTx(numTx).nextTx(nextTx));
                }
            }

            return packet;
        }

        void handletx()
        {
            auto timeMs = connector.timeMs();
            // Get TxFrame if available
            // fmac.417
            auto nextTxFrame = getNextTxFrame(timeMs);
            if (nextTxFrame == nullptr)
            {
                return;
            }

            // fmac.421
            if (neighborTable.size() < MAC_MAXNEIGHBORS_4_TRACKING_2HOP)
            {
                PacketRef(nextTxFrame->data()).forward(true);
            }
            else
            {
                PacketRef(nextTxFrame->data()).forward(false);
            }

            // Send data
            bool success = true;
            if (success)
            {
                txPool.deallocate(*nextTxFrame);
            }
            else
            {
                nextTxFrame->nextTx(timeMs + random.range(FANET_CSMA_MIN, FANET_CSMA_MAX));
            }
        }

    private:
        /**
         * Get the next TX block that can be transmitted.
         * AckPayloadPrioritice ACK, if no ACK found then return any other packet
         * Returns null if nothing can be send
         */
        TxFrame *getNextTxFrame(uint32_t timeMs)
        {
            // Find the ACK packet with the earliest nextTx() time
            auto ackIt = std::min_element(txPool.begin(), txPool.end(), [timeMs](auto &a, auto &b)
                                          {
                                              bool aIsAck = PacketRef(a.data()).type() == Header::MessageType::ACK;
                                              bool bIsAck = PacketRef(b.data()).type() == Header::MessageType::ACK;
        
                                              if (aIsAck && bIsAck) 
                                                  return a.nextTx() < b.nextTx();
                                              if (aIsAck) 
                                                  return true;
                                              return false; });

            if (ackIt != txPool.end() && PacketRef(ackIt->data()).type() == Header::MessageType::ACK && ackIt->nextTx() <= timeMs)
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

        TxFrame *frameInTxPool(const etl::ivector<uint8_t> &buffer)
        {
            auto it = etl::find_if(txPool.begin(), txPool.end(),
                                   [&buffer](auto block)
                                   {
                                       // frame.162
                                       auto a = PacketRef(block.data());
                                       auto b = PacketRef(etl::span<uint8_t>(const_cast<uint8_t *>(buffer.data()), buffer.size()));
                                       if (a.source() != b.source())
                                       {
                                           return false;
                                       }
                                       if (block.data().size() != buffer.size())
                                       {
                                           return false;
                                       }
                                       if (a.destination() != b.destination())
                                       {
                                           return false;
                                       }
                                       if (a.type() != b.type())
                                       {
                                           return false;
                                       }

                                       if (!etl::equal(a.payload(), b.payload()))
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

        // TxFrame *findTxFrameByDestination(Address destination)
        // {
        //     auto it = etl::find_if(txPool.begin(), txPool.end(),
        //                            [&destination](auto block)
        //                            {
        //                                return block.destination == destination;
        //                            });

        //     if (it != txPool.end())
        //     {
        //         return &(*it);
        //     }

        //     return nullptr;
        // }

        // fmac.140
        void removeDeleteAckedFrame(const Address &destination)
        {
            for (auto it = txPool.begin(); it != txPool.end();)
            {
                if (it->destination == destination && PacketRef{it->data()}.ackType() != ExtendedHeader::AckType::NONE)
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
    };

};
