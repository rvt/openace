
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "../fanet/fanet.hpp"
#include "../fanet/protocol.hpp"
#include "etl/vector.h"
#include "helpers.hpp"

using namespace FANET;

uint32_t TICK_TIME = 3;
class TestApp : public Connector {
    public:
        
    virtual uint32_t getTick() const override {
            return TICK_TIME;
        }
    
    virtual void sendFrame(uint8_t codingRate, const etl::span<uint8_t> &data) const override {
        puts("Send Frame");        
    }

    virtual bool isBroadcastReady() const override {
        return true;
    }

};

auto OWN_ADDRESS = Address{0x11,0x1111};
auto OTHER_ADDRESS_55 = Address{0x55,0x5555};
auto OTHER_ADDRESS_66 = Address{0x66,0x6666};
auto OTHER_ADDRESS_UNR = Address{0xEE,0xEEEE};
auto RSS_HIGH = -100;

TEST_CASE("Forwarding Broadcast", "[Protocol]") {
    TestApp app;
    auto protocol = Protocol(app);
    protocol.ownAddress(OWN_ADDRESS);
    protocol.init();
    TrackingPayload payload;
    payload.altitude(1000).climbRate(12);
    TICK_TIME = 3;

    // RVT THIS IS OK
    SECTION("neighborTable") {
        SECTION("Adds in neighborTable", "[Protocol]") {    
            auto v = Packet<1, 1>().source(OTHER_ADDRESS_66).destination(OTHER_ADDRESS_55).payload(payload).build();
            auto result = protocol.handleRx<100, 100>(RSS_HIGH, v);
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
                protocol.handleRx<100, 100>(RSS_HIGH, other);
                REQUIRE(protocol.neighborTable().lastSeen(OTHER_ADDRESS_55) == TICK_TIME);
                REQUIRE(protocol.neighborTable().lastSeen(OTHER_ADDRESS_66) == 0);
            }
        }
    }

    // RVT THIS IS OK
    SECTION("Ignores Own Address") {
        auto v = Packet<1, 1>().source(OWN_ADDRESS).payload(payload).build();
        auto result = protocol.handleRx<100, 100>(RSS_HIGH, v);
        REQUIRE(protocol.pool().getAllocatedBlocks().size() == 0);
        REQUIRE(protocol.neighborTable().size() == 0);
    }

    SECTION("Packet received with Ack Requests") {

        SECTION("Broadcast") {
            SECTION("No Ack requested, should not ack") {
                auto v = Packet<1, 1>().source(OTHER_ADDRESS_55).payload(payload).build();
                auto result = protocol.handleRx<100, 100>(RSS_HIGH, v);
                REQUIRE(protocol.pool().getAllocatedBlocks().size() == 0);    
                REQUIRE(protocol.neighborTable().lastSeen(OTHER_ADDRESS_55) == 3);
            }

            SECTION("Ack over single Hop, should ack without forward") {
                auto v = Packet<1, 1>().source(OTHER_ADDRESS_55).payload(payload).singleHop().build();
                auto result = protocol.handleRx<100, 100>(RSS_HIGH, v);
                REQUIRE(protocol.pool().getAllocatedBlocks().size() == 1);
                REQUIRE(protocol.neighborTable().lastSeen(OTHER_ADDRESS_55) == 3);
                auto poolItem = protocol.pool().begin();
                REQUIRE( makeVectorS<100>(poolItem->data()) == makeVector({0x80, 0x11, 0x11, 0x11, 0x20, 0x55, 0x55, 0x55, }));
            }


            SECTION("Ack over Two Hop, should ack with forward") {
                auto v = Packet<1, 1>().source(OTHER_ADDRESS_55).payload(payload).twoHop().build();
                auto result = protocol.handleRx<100, 100>(RSS_HIGH, v);
                REQUIRE(protocol.pool().getAllocatedBlocks().size() == 1);    
                REQUIRE(protocol.neighborTable().lastSeen(OTHER_ADDRESS_55) == 3);
                auto poolItem = protocol.pool().begin();
                REQUIRE( makeVectorS<100>(poolItem->data()) == makeVector({0xC0, 0x11, 0x11, 0x11, 0x20, 0x55, 0x55, 0x55, }));
            }
        }

        SECTION("Destination to us") {
            SECTION("No Ack requested, should not ack") {
                auto v = Packet<1, 1>().source(OTHER_ADDRESS_55).destination(OWN_ADDRESS).payload(payload).build();
                auto result = protocol.handleRx<100, 100>(0, v);
                REQUIRE(protocol.pool().getAllocatedBlocks().size() == 0);
                REQUIRE(protocol.neighborTable().lastSeen(OTHER_ADDRESS_55) == 3);
            }

            SECTION("Ack over single Hop, should ack without forward") {
                auto v = Packet<1, 1>().source(OTHER_ADDRESS_55).payload(payload).destination(OWN_ADDRESS).singleHop().build();
                auto result = protocol.handleRx<100, 100>(RSS_HIGH, v);
                REQUIRE(protocol.pool().getAllocatedBlocks().size() == 1);
                REQUIRE(protocol.neighborTable().lastSeen(OTHER_ADDRESS_55) == 3);
                auto poolItem = protocol.pool().begin();
                REQUIRE( makeVectorS<100>(poolItem->data()) == makeVector({0x80, 0x11, 0x11, 0x11, 0x20, 0x55, 0x55, 0x55, }));
            }


            SECTION("Ack over Two Hop, should ack and forward") {
                auto v = Packet<1, 1>().source(OTHER_ADDRESS_55).payload(payload).destination(OWN_ADDRESS).twoHop().build();
                auto result = protocol.handleRx<100, 100>(RSS_HIGH, v);
                REQUIRE(protocol.pool().getAllocatedBlocks().size() == 1);
                REQUIRE(protocol.neighborTable().lastSeen(OTHER_ADDRESS_55) == 3);
                auto poolItem = protocol.pool().begin();
                REQUIRE( makeVectorS<100>(poolItem->data()) == makeVector({0xC0, 0x11, 0x11, 0x11, 0x20, 0x55, 0x55, 0x55, }));
            }
        }

        SECTION("Destination not us") {
            SECTION("No Ack requested, should not ack") {
                auto v = Packet<1, 1>().source(OTHER_ADDRESS_66).destination(OTHER_ADDRESS_55).payload(payload).twoHop().build();
                auto result = protocol.handleRx<100, 100>(RSS_HIGH, v);
                REQUIRE(protocol.neighborTable().lastSeen(OTHER_ADDRESS_66) == 3);
                REQUIRE(protocol.pool().getAllocatedBlocks().size() == 0);    
            }
        }    
    }

    // RVT THIS IS OK
    SECTION("Ack Package Received") {
        // Add packet to pool, means we send a packet to a destination and we request ack
        auto packet = Packet<1, 1>().payload(payload).destination(OTHER_ADDRESS_55).singleHop();
        protocol.sendPacket(packet, 0);
    
        SECTION("When ack received for us") {
            auto ack = Packet<1, 1>().source(OTHER_ADDRESS_55).destination(OWN_ADDRESS).buildAck();
            auto result = protocol.handleRx<100, 100>(RSS_HIGH, ack);
            REQUIRE(protocol.pool().getAllocatedBlocks().size() == 0);
        }

        SECTION("When not ack received for us") {
            auto not_ack = Packet<1, 1>().source(OTHER_ADDRESS_55).destination(OWN_ADDRESS).payload(payload).build();
            auto result = protocol.handleRx<100, 100>(RSS_HIGH, not_ack);
            REQUIRE(protocol.pool().getAllocatedBlocks().size() == 1);
        }

        SECTION("When ack received broadcast") {
            auto ack = Packet<1, 1>().source(OTHER_ADDRESS_55).buildAck();
            auto result = protocol.handleRx<100, 100>(RSS_HIGH, ack);
            REQUIRE(protocol.pool().getAllocatedBlocks().size() == 0);
        }

        SECTION("When ack received for us from other") {
            auto ack = Packet<1, 1>().source(OTHER_ADDRESS_66).destination(OWN_ADDRESS).buildAck();
            auto result = protocol.handleRx<100, 100>(RSS_HIGH, ack);
            REQUIRE(protocol.pool().getAllocatedBlocks().size() == 1);
        }

        SECTION("When ack received for other") {
            auto ack = Packet<1, 1>().source(OTHER_ADDRESS_55).destination(OTHER_ADDRESS_UNR).buildAck();
            auto result = protocol.handleRx<100, 100>(RSS_HIGH, ack);
            REQUIRE(protocol.pool().getAllocatedBlocks().size() == 1);
        }
    }

    // WE NEED TO CONTINUE HERE
    SECTION("Packet Forwarding Unicast") {
    //     SECTION("Seen, forward without ack ") {
    //         auto v = Packet<1, 1>().source(Address{0x14, 0x9999}).payload(payload).destination(Address{0x45, 0x6723}).forward().build();
    //         // Add neighbor
    //         auto seenItem = makeVector({0x80, 0x45, 0x23, 0x67, 0x20, 0x99, 0x99, 0x99, });
    //         protocol.handleRx<100, 100>(0, seenItem);

    //         auto result = protocol.handleRx<100, 100>(-100, v);
    //         REQUIRE(protocol.pool().getAllocatedBlocks().size() == 1);

    //         auto poolItem = protocol.pool().begin();
    //         REQUIRE(poolItem->numTx() == 0);
    //         REQUIRE(poolItem->nextTx() >= 103);
    //         REQUIRE( makeVectorS<100>(poolItem->data()) == makeVector({0x81, 0x14, 0x99, 0x99, 0x20, 0x45, 0x23, 0x67, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xE8, 0x03, 0x00, 0x98, 0x00,  }));
    //     }

    //     SECTION("Not Seen, should not add to queue") {
    //         auto v = Packet<1, 1>().source(Address{0x14, 0x9999}).payload(payload).destination(Address{0x45, 0x6723}).build();
    //         auto result = protocol.handleRx<100, 100>(-100, v);
    //         REQUIRE(protocol.pool().getAllocatedBlocks().size() == 0);    
    //     }

    //     SECTION("Seen, rssI to low should not add to queue") {
    //         auto v = Packet<1, 1>().source(Address{0x14, 0x9999}).payload(payload).destination(Address{0x45, 0x6723}).build();
    //         auto seenItem = makeVector({0x80, 0x45, 0x23, 0x67, 0x20, 0x99, 0x99, 0x99, });
    //         protocol.handleRx<100, 100>(0, seenItem);

    //         auto result = protocol.handleRx<100, 100>(-70, v);
    //         REQUIRE(protocol.pool().getAllocatedBlocks().size() == 0);
    //     }
    }


    
}
