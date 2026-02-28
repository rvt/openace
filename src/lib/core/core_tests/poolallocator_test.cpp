
#include <catch2/catch_test_macros.hpp>
#include "../ace/poolallocator.hpp"
#include "semphr.h"



using TestAlloc = MultiPoolAllocator<
    PoolSpec<32, 2>,
    PoolSpec<64, 2>,
    PoolSpec<160, 1>>;

TEST_CASE("Nobody owns nullptr", "[poolalloc]")
{
    TestAlloc alloc;
    void *p = nullptr;
    REQUIRE_FALSE(alloc.owns_pool<0>(p));
    REQUIRE_FALSE(alloc.owns_pool<1>(p));
    REQUIRE_FALSE(alloc.owns_pool<2>(p));
}

TEST_CASE("alloc uses smallest fitting pool", "[poolalloc]")
{
    TestAlloc alloc;

    void *p1 = alloc.alloc(10);
    void *p2 = alloc.alloc(30);
    void *p3 = alloc.alloc(33);

    // p1 and p2 must come from 32 pool
    REQUIRE(alloc.owns_pool<0>(p1));
    REQUIRE(alloc.owns_pool<0>(p2));

    // p3 must come from 64 pool
    REQUIRE(alloc.owns_pool<1>(p3));
}

TEST_CASE("realloc stays in same pool when size fits", "[poolalloc]")
{
    TestAlloc alloc;

    void *p = alloc.alloc(20);
    void *p2 = alloc.realloc(p, 30);

    REQUIRE(alloc.owns_pool<0>(p2));
    REQUIRE(p2 == p);
}

TEST_CASE("realloc preserves content when moving to larger pool", "[poolalloc]")
{
    TestAlloc alloc;

    // Allocate from 32-byte pool
    uint8_t *p = static_cast<uint8_t *>(alloc.alloc(20));
    REQUIRE(p != nullptr);

    // Fill with known pattern
    for (size_t i = 0; i < 20; ++i)
    {
        p[i] = static_cast<uint8_t>(0xA0 + i);
    }

    // Grow -> must move to 64-byte pool
    uint8_t *p2 = static_cast<uint8_t *>(alloc.realloc(p, 40));
    REQUIRE(p2 != nullptr);
    REQUIRE(p2 != p);

    // Verify content preserved
    for (size_t i = 0; i < 20; ++i)
    {
        REQUIRE(p2[i] == static_cast<uint8_t>(0xA0 + i));
    }
}

TEST_CASE("release returns memory to pool", "[poolalloc]")
{
    TestAlloc alloc;

    void* p1 = alloc.alloc(20);
    REQUIRE(p1 != nullptr);

    void* p2 = alloc.alloc(20);
    REQUIRE(p2 != nullptr);

    // PoolSpec<32, 2> -> now full

    void* p3 = alloc.alloc(20);

    REQUIRE(p3 == nullptr); // pool exhausted

    alloc.release(p1);

    // Now there should be space again
    void* p4 = alloc.alloc(20);
    REQUIRE(p4 != nullptr);
}

TEST_CASE("alloc returns nullptr when pool exhausted", "[poolalloc]")
{
    TestAlloc alloc;

    void *p1 = alloc.alloc(160);
    REQUIRE(alloc.owns_pool<2>(p1));

    void *p2 = alloc.alloc(160);
    REQUIRE(p2 == nullptr); // only 1 block in this pool
}
