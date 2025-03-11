
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "../fanet/fanet.hpp"
#include "../fanet/protocol.hpp"
#include "etl/vector.h"
#include "etl/random.h"
#include "etl/span.h"
#include "helpers.hpp"
#include "../fanet/blockAllocator.hpp"
#include <random>

using namespace FANET;

class TestData {
public:
    int id;
    uint8_t type;
    etl::span<uint8_t> block;

    etl::span<uint8_t> data() const {
        return block;
    }
    void data(etl::span<uint8_t> v)
    {
        block = v;
    }
};

TEST_CASE("Queue tests Monkey", "[BlockAllocator]")
{
    BlockAllocator<TestData, 50, 12> test;
    uint8_t externalArray[1024];

    std::mt19937 rng(234234);
    std::uniform_int_distribution<int> gen(4, 255);
    std::uniform_int_distribution<int> binaryGen(0, 1);
    std::uniform_int_distribution<int> shiftGen(0, 4);

    // Seed the random number generator for deallocation randomness
    int count = 0;

    bool allocated;
    for (int i = 0; i < 100000; i++)
    {
        // Full uo queue untill it's full
        do
        {
            count++;
            uint8_t type = gen(rng) >> shiftGen(rng);
            etl::vector_ext<uint8_t> vec(externalArray, type);
            vec.resize(type);
            vec.fill(type);

            allocated = test.add(TestData{count, type, vec});
        } while (allocated);
        // printf("Full:    ");
        // test.printAllocationMap();

        // Test the queue
        for (auto b : test.getAllocatedBlocks())
        {
            etl::vector_ext<uint8_t> vec(externalArray, b.type);
            vec.resize(b.type);
            vec.fill(b.type);
            etl::span<uint8_t> data(vec.begin(), vec.end());
            if (!etl::equal(b.block, data))
            {
                printf("id: %d type: %d\n", b.id, b.type);
                REQUIRE(b.block == data);
            }
        }

        // Randomly delete some blocks
        for (auto it = test.begin(); it != test.end();)
        {
            if (binaryGen(rng)) 
            {
                it = test.remove(it);
            }
            else
            {
                ++it; // Move to the next element if not deleted
            }
        }
        // printf("Deleted: ");
        // test.printAllocationMap();
    }
}