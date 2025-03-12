
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "../fanet/fanet.hpp"
#include "../fanet/protocol.hpp"
#include "etl/vector.h"
#include "helpers.hpp"

using namespace FANET;

uint32_t TICK_TIME = 3;
auto OWN_ADDRESS = Address{0x11,0x1111};
auto OTHER_ADDRESS_55 = Address{0x55,0x5555};
auto OTHER_ADDRESS_66 = Address{0x66,0x6666};
auto OTHER_ADDRESS_UNR = Address{0xEE,0xEEEE};
auto BROADCAST_ADDRESS = Address{};
auto IGNORING_ADDRESS = Address{0xff, 0xffff};
auto RSSI_HIGH = -100;
auto RSSI_LOW = -70;

class TestProtocol : public Protocol
{
public:
    TestProtocol(Connector &connector_) : Protocol(connector_)
    {
    }

    void setAirTime(float time)
    {
        airTime(time);
    }
};

class TestApp : public Connector
{
public:
    uint16_t receivedAckId = 0;
    uint32_t receivedAckTotal = 0;

    virtual uint32_t getTick() const override
    {
        return TICK_TIME;
    }

    virtual void ackReceived(uint16_t id) override
    {
        printf("============= > Ack Received %d\n", id);
        receivedAckId = id;
        receivedAckTotal++;
    }

    virtual void sendFrame(uint8_t codingRate, const etl::span<uint8_t> &data) const override
    {
        puts("Send Frame");
    }

    virtual bool isBroadcastReady() const override
    {
        return true;
    }
};

const FANET::TxFrame * findByAddress(Protocol &protocol, Header::MessageType type, Address destination, Address source) {
    auto it = etl::find_if(protocol.pool().begin(), protocol.pool().end(),
    [&type, &destination, &source](auto block)
    {
        if (block.type() != type)
        {
            return false;
        }

        if (block.destination() != destination && destination != IGNORING_ADDRESS)
        {
            return false;
        }

        if (block.source() != source && source != IGNORING_ADDRESS)
        {
            return false;
        }

        return true;
    });

    if (it != protocol.pool().end())
    {
        return &(*it);
    }

    return nullptr;
}

const FANET::TxFrame * findByAddress(Protocol &protocol, Address destination, Address source = IGNORING_ADDRESS) {
    auto it = etl::find_if(protocol.pool().begin(), protocol.pool().end(),
    [&destination, &source](auto block)
    {
        if (block.destination() != destination && destination != IGNORING_ADDRESS)
        {
            return false;
        }

        if (block.source() != source && source != IGNORING_ADDRESS)
        {
            return false;
        }

        return true;
    });

    if (it != protocol.pool().end())
    {
        return &(*it);
    }

    return nullptr;
}

TEST_CASE("Forwarding Broadcast", "[Protocol]") {
    TestApp app;
    auto protocol = TestProtocol(app);
    protocol.ownAddress(OWN_ADDRESS);
    protocol.init();
    TrackingPayload payload;
    payload.altitude(1000).climbRate(12);
    TICK_TIME = 3;

    SECTION("neighborTable") {
        SECTION("Adds in neighborTable", "[Protocol]") {    
            auto v = Packet<1, 1>().source(OTHER_ADDRESS_66).destination(OTHER_ADDRESS_55).payload(payload).build();
            protocol.handleRx<100, 100>(RSSI_HIGH, v);
            REQUIRE(protocol.neighborTable().lastSeen(OTHER_ADDRESS_66) == 3);
            
            SECTION("Update last seen") {
                TICK_TIME = 10;
                protocol.handleRx<100, 100>(0, v);

                REQUIRE(protocol.neighborTable().lastSeen(OTHER_ADDRESS_66) == 10);
                REQUIRE(protocol.neighborTable().size() == 1);    
            }

            SECTION("Cleanup called") {
                TICK_TIME = 20 + (4 * 60 * 1000 + 10'000); // NEIGHBOR_MAX_TIMEOUT_MS
                auto other = Packet<1, 1>().source(OTHER_ADDRESS_55).destination(OTHER_ADDRESS_66).payload(payload).build();
                protocol.handleRx<100, 100>(RSSI_HIGH, other);

                REQUIRE(protocol.neighborTable().lastSeen(OTHER_ADDRESS_55) == TICK_TIME);
                REQUIRE(protocol.neighborTable().lastSeen(OTHER_ADDRESS_66) == 0);
            }
        }
    }

    SECTION("Ignores Own Address") {
        auto v = Packet<1, 1>().source(OWN_ADDRESS).payload(payload).build();
        protocol.handleRx<100, 100>(RSSI_HIGH, v);
        REQUIRE(protocol.pool().getAllocatedBlocks().size() == 0);
        REQUIRE(protocol.neighborTable().size() == 0);
    }

    SECTION("Init should clean ") {
        auto v = Packet<1, 1>().source(OTHER_ADDRESS_55).payload(payload).build();
        protocol.handleRx<100, 100>(RSSI_HIGH, v);
        REQUIRE(protocol.neighborTable().size() == 1);
        auto packet = Packet<1, 1>().payload(payload).destination(OTHER_ADDRESS_55).singleHop();
        protocol.sendPacket(packet, 0, true);
        REQUIRE(protocol.pool().getAllocatedBlocks().size() == 1);

        protocol.init();
        REQUIRE(protocol.pool().getAllocatedBlocks().size() == 0);
        REQUIRE(protocol.neighborTable().size() == 0);
    }

    SECTION("Ack response") {
        SECTION("Broadcast") {
            SECTION("No Ack requested, should not ack") {
                auto v = Packet<1, 1>().source(OTHER_ADDRESS_55).payload(payload).build();
                protocol.handleRx<100, 100>(RSSI_HIGH, v);

                REQUIRE(protocol.pool().getAllocatedBlocks().size() == 0);
                REQUIRE(protocol.neighborTable().lastSeen(OTHER_ADDRESS_55) == 3);
            }

            SECTION("Ack over single Hop, should ack without forward") {
                auto v = Packet<1, 1>().source(OTHER_ADDRESS_55).payload(payload).singleHop().build();
                protocol.handleRx<100, 100>(RSSI_HIGH, v);

                auto poolItem = findByAddress(protocol, OTHER_ADDRESS_55, OWN_ADDRESS);
                REQUIRE(protocol.pool().getAllocatedBlocks().size() == 1);
                REQUIRE(protocol.neighborTable().lastSeen(OTHER_ADDRESS_55) == 3);
                REQUIRE( makeVectorS<100>(poolItem->data()) == makeVector({0x80, 0x11, 0x11, 0x11, 0x20, 0x55, 0x55, 0x55, }));
            }

            SECTION("Ack over Two Hop, should ack with forward") {
                auto v = Packet<1, 1>().source(OTHER_ADDRESS_55).payload(payload).twoHop().build();
                protocol.handleRx<100, 100>(RSSI_HIGH, v);

                auto poolItem = findByAddress(protocol, OTHER_ADDRESS_55, OWN_ADDRESS);
                REQUIRE(protocol.pool().getAllocatedBlocks().size() == 1);    
                REQUIRE(protocol.neighborTable().lastSeen(OTHER_ADDRESS_55) == 3);
                REQUIRE( makeVectorS<100>(poolItem->data()) == makeVector({0xC0, 0x11, 0x11, 0x11, 0x20, 0x55, 0x55, 0x55, }));
            }
        }

        SECTION("Unicast") {
            SECTION("No Ack requested, should not ack") {
                auto v = Packet<1, 1>().source(OTHER_ADDRESS_55).destination(OWN_ADDRESS).payload(payload).build();
                protocol.handleRx<100, 100>(0, v);

                REQUIRE(protocol.pool().getAllocatedBlocks().size() == 0);
                REQUIRE(protocol.neighborTable().lastSeen(OTHER_ADDRESS_55) == 3);
            }

            SECTION("Ack over single Hop, should ack without forward") {
                auto v = Packet<1, 1>().source(OTHER_ADDRESS_55).payload(payload).destination(OWN_ADDRESS).singleHop().build();
                protocol.handleRx<100, 100>(RSSI_HIGH, v);

                auto poolItem = findByAddress(protocol, OTHER_ADDRESS_55, OWN_ADDRESS);
                REQUIRE(protocol.pool().getAllocatedBlocks().size() == 1);
                REQUIRE(protocol.neighborTable().lastSeen(OTHER_ADDRESS_55) == 3);
                REQUIRE( makeVectorS<100>(poolItem->data()) == makeVector({0x80, 0x11, 0x11, 0x11, 0x20, 0x55, 0x55, 0x55, }));
            }

            SECTION("Ack over Two Hop, should ack and forward") {
                auto v = Packet<1, 1>().source(OTHER_ADDRESS_55).payload(payload).destination(OWN_ADDRESS).twoHop().build();
                protocol.handleRx<100, 100>(RSSI_HIGH, v);

                auto poolItem = findByAddress(protocol, OTHER_ADDRESS_55, OWN_ADDRESS);
                REQUIRE(protocol.pool().getAllocatedBlocks().size() == 1);
                REQUIRE(protocol.neighborTable().lastSeen(OTHER_ADDRESS_55) == 3);
                REQUIRE( makeVectorS<100>(poolItem->data()) == makeVector({0xC0, 0x11, 0x11, 0x11, 0x20, 0x55, 0x55, 0x55, }));
            }
        }
    }

    SECTION("Ack Received") {
        SECTION("With Packed in pool") {
            // Add packet to pool, means we send a packet to a destination and we request ack
            auto packet = Packet<1, 1>().payload(payload).destination(OTHER_ADDRESS_55).singleHop();
            protocol.sendPacket(packet, 10, true);
            packet = Packet<1, 1>().payload(payload).destination(OTHER_ADDRESS_66).singleHop();
            protocol.sendPacket(packet, 11, true);
            REQUIRE(protocol.pool().getAllocatedBlocks().size() == 2);

            SECTION("When ack received for us, should remove the packages") {
                auto ack = Packet<1, 1>().source(OTHER_ADDRESS_55).destination(OWN_ADDRESS).buildAck();
                protocol.handleRx<100, 100>(RSSI_HIGH, ack);

                REQUIRE(protocol.pool().getAllocatedBlocks().size() == 1);
                REQUIRE( findByAddress(protocol, OTHER_ADDRESS_55) == nullptr );
                REQUIRE( findByAddress(protocol, OTHER_ADDRESS_66) != nullptr );
                REQUIRE( app.receivedAckId == 10 );
                REQUIRE( app.receivedAckTotal == 1);
            }

            SECTION("When ack received broadcast should remove packages") {
                auto ack = Packet<1, 1>().source(OTHER_ADDRESS_55).buildAck();
                protocol.handleRx<100, 100>(RSSI_HIGH, ack);

                REQUIRE(protocol.pool().getAllocatedBlocks().size() == 1);
                REQUIRE( findByAddress(protocol, OTHER_ADDRESS_55) == nullptr );
                REQUIRE( findByAddress(protocol, OTHER_ADDRESS_66) != nullptr );
                REQUIRE( app.receivedAckId == 10 );
                REQUIRE( app.receivedAckTotal == 1);
            }

            SECTION("When not ack received for us") {
                auto not_ack = Packet<1, 1>().source(OTHER_ADDRESS_55).destination(OWN_ADDRESS).payload(payload).build();
                protocol.handleRx<100, 100>(RSSI_HIGH, not_ack);

                REQUIRE( findByAddress(protocol, OTHER_ADDRESS_55) != nullptr );
                REQUIRE( findByAddress(protocol, OTHER_ADDRESS_66) != nullptr );
            }
        
            SECTION("When ack received broadcast with forward, should remove forward and ack") {
                auto ack = Packet<1, 1>().source(OTHER_ADDRESS_55).forward().buildAck();
                protocol.handleRx<100, 100>(RSSI_HIGH, ack);

                auto poolItem = findByAddress(protocol, BROADCAST_ADDRESS, OTHER_ADDRESS_55);
                REQUIRE( findByAddress(protocol, OTHER_ADDRESS_66) != nullptr );

                // Should we really send a broadcast like this????
                REQUIRE( makeVectorS<100>(poolItem->data()) == makeVector({0x00, 0x55, 0x55, 0x55, }));    
                REQUIRE( app.receivedAckId == 10 );
                REQUIRE( app.receivedAckTotal == 1);
            }

            SECTION("When ack received for other") {
                auto ack = Packet<1, 1>().source(OTHER_ADDRESS_55).destination(OTHER_ADDRESS_UNR).buildAck();
                protocol.handleRx<100, 100>(RSSI_HIGH, ack);

                REQUIRE( findByAddress(protocol, OTHER_ADDRESS_55) != nullptr );
                REQUIRE( findByAddress(protocol, OTHER_ADDRESS_66) != nullptr );
            }
        }

        SECTION("When ack received seen with forward, should forward ack package") {
            // Add to pool so we have seen OTHER_ADDRESS_66
            auto packet = Packet<1, 1>().payload(payload).source(OTHER_ADDRESS_66).build();
            protocol.handleRx<100, 100>(RSSI_HIGH, packet);
            
            auto ack = Packet<1, 1>().source(OTHER_ADDRESS_55).destination(OTHER_ADDRESS_66).forward().buildAck();
            protocol.handleRx<100, 100>(RSSI_HIGH, ack);

            auto poolItem = findByAddress(protocol, Header::MessageType::ACK, OTHER_ADDRESS_66, OTHER_ADDRESS_55);
            REQUIRE( makeVectorS<100>(poolItem->data()) == makeVector({0x80, 0x55, 0x55, 0x55, 0x20, 0x66, 0x66, 0x66 }));    
        }

        SECTION("When ack received not seen with forward, should not forward") {
            auto ack = Packet<1, 1>().source(OTHER_ADDRESS_55).destination(OTHER_ADDRESS_UNR).forward().buildAck();
            protocol.handleRx<100, 100>(RSSI_HIGH, ack);
            REQUIRE(protocol.pool().getAllocatedBlocks().size() == 0);
        }
    }

    SECTION("Packet Forwarding Unicast") {
        auto SEENPACKET = Packet<1, 1>().source(OTHER_ADDRESS_66).destination(OTHER_ADDRESS_55).payload(payload).build();

        auto FORWARDPACKETUNI = Packet<1, 1>().source(OTHER_ADDRESS_UNR).destination(OTHER_ADDRESS_66).payload(payload).forward().build();
        auto FORWARDPACKETUNIONEHOP = Packet<1, 1>().source(OTHER_ADDRESS_UNR).destination(OTHER_ADDRESS_66).payload(payload).forward().ack(ExtendedHeader::AckType::SINGLEHOP).build();
        auto FORWARDPACKETBROADCAST = Packet<1, 1>().source(OTHER_ADDRESS_UNR).payload(payload).forward().build();


        SECTION("Seen, forward without ack, low RSSI ") {
            protocol.handleRx<100, 100>(RSSI_HIGH, SEENPACKET);

            SECTION("Should not forward, not seen") {
                protocol.handleRx<100, 100>(RSSI_LOW, FORWARDPACKETUNI);
                REQUIRE(protocol.pool().getAllocatedBlocks().size() == 0);
            }

            SECTION("Should forward unicast") {
                protocol.handleRx<100, 100>(RSSI_HIGH, FORWARDPACKETUNI);
                auto poolItem = findByAddress(protocol, OTHER_ADDRESS_66, OTHER_ADDRESS_UNR);
                REQUIRE(protocol.pool().getAllocatedBlocks().size() == 1);
                REQUIRE(poolItem->numTx() == 0);
                REQUIRE(poolItem->nextTx() >= 103);
                REQUIRE(poolItem->rssi() >= RSSI_HIGH);
                REQUIRE( makeVectorS<100>(poolItem->data()) == makeVector({0x81, 0xEE, 0xEE, 0xEE, 0x20, 0x66, 0x66, 0x66, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xE8, 0x03, 0x00, 0x98, 0x00,  }));    

                SECTION("Should drop package with LOW RSSI") {
                    protocol.handleRx<100, 100>(RSSI_LOW, FORWARDPACKETUNI);
                    REQUIRE(protocol.pool().getAllocatedBlocks().size() == 0);
                }

                SECTION("Should adjust departure time") {
                    TICK_TIME = 5000;
                    protocol.handleRx<100, 100>(RSSI_HIGH, FORWARDPACKETUNI);
                    auto poolItem = findByAddress(protocol, OTHER_ADDRESS_66, OTHER_ADDRESS_UNR);
                    REQUIRE(poolItem->nextTx() >= 5000);
                }

                SECTION("Should not adjust departure time with different payload") {
                    auto payloadDifferent = payload;;
                    payloadDifferent.climbRate(13);            
                    auto DIFFERENT = Packet<1, 1>().source(OTHER_ADDRESS_UNR).destination(OTHER_ADDRESS_66).payload(payloadDifferent).forward().build();

                    TICK_TIME = 5000;
                    protocol.handleRx<100, 100>(RSSI_HIGH, DIFFERENT);
                    auto poolItem = findByAddress(protocol, OTHER_ADDRESS_66, OTHER_ADDRESS_UNR);
                    REQUIRE(poolItem->nextTx() < 2000);
                }
            }

            SECTION("Should forward unicast with hop") {
                protocol.handleRx<100, 100>(RSSI_HIGH, FORWARDPACKETUNIONEHOP);
                auto poolItem = findByAddress(protocol, OTHER_ADDRESS_66, OTHER_ADDRESS_UNR);
                REQUIRE(poolItem->numTx() == 1);
            }

            SECTION("Should forward broadcast") {
                protocol.handleRx<100, 100>(RSSI_HIGH, FORWARDPACKETBROADCAST);
                auto poolItem = findByAddress(protocol, IGNORING_ADDRESS, OTHER_ADDRESS_UNR);
                REQUIRE(protocol.pool().getAllocatedBlocks().size() == 1);                
                REQUIRE(poolItem->numTx() == 0);
                REQUIRE(poolItem->nextTx() >= 103);
                REQUIRE( makeVectorS<100>(poolItem->data()) == makeVector({0x01, 0xEE, 0xEE, 0xEE, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xE8, 0x03, 0x00, 0x98, 0x00, }));    
            }


            SECTION("Should not forward due to high airtime") {
                protocol.setAirTime(1.0f);
                protocol.handleRx<100, 100>(RSSI_HIGH, FORWARDPACKETUNI);
                REQUIRE(protocol.pool().getAllocatedBlocks().size() == 0);
            }
        }

        SECTION("Not Seen, should not add to queue") {
            protocol.handleRx<100, 100>(RSSI_HIGH, FORWARDPACKETUNI);

            REQUIRE(protocol.pool().getAllocatedBlocks().size() == 0);
        }
    }
    
}
