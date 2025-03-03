
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "../fanet/fanet.hpp"
#include "../fanet/protocol.hpp"
#include "etl/vector.h"
#include "helpers.hpp"

using namespace FANET;


class TestApp : public Connector {
    public:
        
    virtual uint32_t timeMs() const override {
            return 0;
        }
};

TEST_CASE("Protocol Default Constructor", "[TrackingPayload]") {

    TestApp app;
    Protocol p(app);

    auto result = p.handleRx<100, 100>(0, etl::vector<uint8_t, 255>());

}
