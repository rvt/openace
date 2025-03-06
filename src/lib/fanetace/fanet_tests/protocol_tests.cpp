
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "../fanet/fanet.hpp"
#include "../fanet/protocol.hpp"
#include "etl/vector.h"
#include "helpers.hpp"

using namespace FANET;


class TestApp : public Connector {
    public:
        
    virtual uint32_t getTick() const override {
            return 0;
        }
    
    virtual void sendFrame(uint8_t codingRate, const etl::span<uint8_t> &data) const override {
        puts("Send Frame");        
    }

    virtual bool isBroadcastReady() const override {
        return true;
    }

};

TEST_CASE("Protocol Default Constructor", "[TrackingPayload]") {

    TestApp app;
    Protocol p(app);

    auto v = etl::vector<uint8_t, 255>();
    auto result = p.handleRx<100, 100>(0, v);

}
