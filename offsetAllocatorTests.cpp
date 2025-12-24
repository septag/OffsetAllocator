#define CATCH_CONFIG_RUNNER
#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>

#define OFFSET_ALLOCATOR_IMPLEMENT
#include "offsetAllocator.h"

static constexpr uint32_t MAX_SIZE = 1024 * 1024 * 256;
static constexpr uint32_t MAX_ALLOCS = 128 * 1024;

namespace offsetAllocatorTests
{
    TEST_CASE("numbers", "[SmallFloat]")
    {
        SECTION("uintToFloat")
        {
            // Denorms, exp=1 and exp=2 + mantissa = 0 are all precise.
            // NOTE: Assuming 8 value (3 bit) mantissa.
            // If this test fails, please change this assumption!
            uint32_t preciseNumberCount = 17;
            for (uint32_t i = 0; i < preciseNumberCount; i++)
            {
                uint32_t roundUp = OffsetAllocator_UintToFloatRoundUp(i);
                uint32_t roundDown = OffsetAllocator_UintToFloatRoundDown(i);
                REQUIRE(i == roundUp);
                REQUIRE(i == roundDown);
            }
            
            // Test some random picked numbers
            struct NumberFloatUpDown
            {
                uint32_t number;
                uint32_t up;
                uint32_t down;
            };
            
            NumberFloatUpDown testData[] = {
                {.number = 17, .up = 17, .down = 16},
                {.number = 118, .up = 39, .down = 38},
                {.number = 1024, .up = 64, .down = 64},
                {.number = 65536, .up = 112, .down = 112},
                {.number = 529445, .up = 137, .down = 136},
                {.number = 1048575, .up = 144, .down = 143},
            };
            
            for (uint32_t i = 0; i < sizeof(testData) / sizeof(NumberFloatUpDown); i++)
            {
                NumberFloatUpDown v = testData[i];
                uint32_t roundUp = OffsetAllocator_UintToFloatRoundUp(v.number);
                uint32_t roundDown = OffsetAllocator_UintToFloatRoundDown(v.number);
                REQUIRE(roundUp == v.up);
                REQUIRE(roundDown == v.down);
            }
        }
        
        SECTION("floatToUint")
        {
            // Denorms, exp=1 and exp=2 + mantissa = 0 are all precise.
            // NOTE: Assuming 8 value (3 bit) mantissa.
            // If this test fails, please change this assumption!
            uint32_t preciseNumberCount = 17;
            for (uint32_t i = 0; i < preciseNumberCount; i++)
            {
                uint32_t v = OffsetAllocator_FloatToUint(i);
                REQUIRE(i == v);
            }
            
            // Test that float->uint->float conversion is precise for all numbers
            // NOTE: Test values < 240. 240->4G = overflows 32 bit integer
            for (uint32_t i = 0; i < 240; i++)
            {
                uint32_t v = OffsetAllocator_FloatToUint(i);
                uint32_t roundUp = OffsetAllocator_UintToFloatRoundUp(v);
                uint32_t roundDown = OffsetAllocator_UintToFloatRoundDown(v);
                REQUIRE(i == roundUp);
                REQUIRE(i == roundDown);
                //if ((i%8) == 0) printf("\n");
                //printf("%u->%u ", i, v);
            }
        }
    }

    TEST_CASE("basic", "[offsetAllocator]")
    {
        size_t bufferSize = OffsetAllocator_GetRequiredBytes(MAX_ALLOCS);
        void* buffer = malloc(bufferSize);
        OffsetAllocator* allocator = OffsetAllocator_Create(MAX_SIZE, MAX_ALLOCS, buffer, bufferSize);
        OffsetAllocatorAllocation a;
        OffsetAllocator_Allocate(allocator, 1337, &a);
        REQUIRE(a.offset == 0);
        OffsetAllocator_Free(allocator, &a);
        free(buffer);
    }

    TEST_CASE("allocate", "[offsetAllocator]")
    {
        size_t bufferSize = OffsetAllocator_GetRequiredBytes(MAX_ALLOCS);
        void* buffer = malloc(bufferSize);
        OffsetAllocator* allocator = OffsetAllocator_Create(MAX_SIZE, MAX_ALLOCS, buffer, bufferSize);

        SECTION("simple")
        {
            // Free merges neighbor empty nodes. Next allocation should also have offset = 0
            OffsetAllocatorAllocation a;
            OffsetAllocator_Allocate(allocator, 0, &a);
            REQUIRE(a.offset == 0);
            
            OffsetAllocatorAllocation b;
            OffsetAllocator_Allocate(allocator, 1, &b);
            REQUIRE(b.offset == 0);

            OffsetAllocatorAllocation c;
            OffsetAllocator_Allocate(allocator, 123, &c);
            REQUIRE(c.offset == 1);

            OffsetAllocatorAllocation d;
            OffsetAllocator_Allocate(allocator, 1234, &d);
            REQUIRE(d.offset == 124);

            OffsetAllocator_Free(allocator, &a);
            OffsetAllocator_Free(allocator, &b);
            OffsetAllocator_Free(allocator, &c);
            OffsetAllocator_Free(allocator, &d);
            
            // End: Validate that allocator has no fragmentation left. Should be 100% clean.
            OffsetAllocatorAllocation validateAll;
            OffsetAllocator_Allocate(allocator, MAX_SIZE, &validateAll);
            REQUIRE(validateAll.offset == 0);
            OffsetAllocator_Free(allocator, &validateAll);
        }

        SECTION("merge trivial")
        {
            // Free merges neighbor empty nodes. Next allocation should also have offset = 0
            OffsetAllocatorAllocation a;
            OffsetAllocator_Allocate(allocator, 1337, &a);
            REQUIRE(a.offset == 0);
            OffsetAllocator_Free(allocator, &a);
            
            OffsetAllocatorAllocation b;
            OffsetAllocator_Allocate(allocator, 1337, &b);
            REQUIRE(b.offset == 0);
            OffsetAllocator_Free(allocator, &b);
            
            // End: Validate that allocator has no fragmentation left. Should be 100% clean.
            OffsetAllocatorAllocation validateAll;
            OffsetAllocator_Allocate(allocator, MAX_SIZE, &validateAll);
            REQUIRE(validateAll.offset == 0);
            OffsetAllocator_Free(allocator, &validateAll);
        }
        
        SECTION("reuse trivial")
        {
            // Allocator should reuse node freed by A since the allocation C fits in the same bin (using pow2 size to be sure)
            OffsetAllocatorAllocation a;
            OffsetAllocator_Allocate(allocator, 1024, &a);
            REQUIRE(a.offset == 0);

            OffsetAllocatorAllocation b;
            OffsetAllocator_Allocate(allocator, 3456, &b);
            REQUIRE(b.offset == 1024);

            OffsetAllocator_Free(allocator, &a);
            
            OffsetAllocatorAllocation c;
            OffsetAllocator_Allocate(allocator, 1024, &c);
            REQUIRE(c.offset == 0);

            OffsetAllocator_Free(allocator, &c);
            OffsetAllocator_Free(allocator, &b);
            
            // End: Validate that allocator has no fragmentation left. Should be 100% clean.
            OffsetAllocatorAllocation validateAll;
            OffsetAllocator_Allocate(allocator, MAX_SIZE, &validateAll);
            REQUIRE(validateAll.offset == 0);
            OffsetAllocator_Free(allocator, &validateAll);
        }

        SECTION("reuse complex")
        {
            // Allocator should not reuse node freed by A since the allocation C doesn't fits in the same bin
            // However node D and E fit there and should reuse node from A
            OffsetAllocatorAllocation a;
            OffsetAllocator_Allocate(allocator, 1024, &a);
            REQUIRE(a.offset == 0);

            OffsetAllocatorAllocation b;
            OffsetAllocator_Allocate(allocator, 3456, &b);
            REQUIRE(b.offset == 1024);

            OffsetAllocator_Free(allocator, &a);
            
            OffsetAllocatorAllocation c;
            OffsetAllocator_Allocate(allocator, 2345, &c);
            REQUIRE(c.offset == 1024 + 3456);

            OffsetAllocatorAllocation d;
            OffsetAllocator_Allocate(allocator, 456, &d);
            REQUIRE(d.offset == 0);

            OffsetAllocatorAllocation e;
            OffsetAllocator_Allocate(allocator, 512, &e);
            REQUIRE(e.offset == 456);

            OffsetAllocatorStorageReport report;
            OffsetAllocator_GetStorageReport(allocator, &report);
            REQUIRE(report.totalFreeSpace == 1024 * 1024 * 256 - 3456 - 2345 - 456 - 512);
            REQUIRE(report.largestFreeRegion != report.totalFreeSpace);

            OffsetAllocator_Free(allocator, &c);
            OffsetAllocator_Free(allocator, &d);
            OffsetAllocator_Free(allocator, &b);
            OffsetAllocator_Free(allocator, &e);
            
            // End: Validate that allocator has no fragmentation left. Should be 100% clean.
            OffsetAllocatorAllocation validateAll;
            OffsetAllocator_Allocate(allocator, MAX_SIZE, &validateAll);
            REQUIRE(validateAll.offset == 0);
            OffsetAllocator_Free(allocator, &validateAll);
        }
        
        SECTION("zero fragmentation")
        {
            // Allocate 256x 1MB. Should fit. Then free four random slots and reallocate four slots.
            // Plus free four contiguous slots an allocate 4x larger slot. All must be zero fragmentation!
            OffsetAllocatorAllocation allocations[256];
            for (uint32_t i = 0; i < 256; i++)
            {
                OffsetAllocator_Allocate(allocator, 1024 * 1024, &allocations[i]);
                REQUIRE(allocations[i].offset == i * 1024 * 1024);
            }

            OffsetAllocatorStorageReport report;
            OffsetAllocator_GetStorageReport(allocator, &report);
            REQUIRE(report.totalFreeSpace == 0);
            REQUIRE(report.largestFreeRegion == 0);

            // Free four random slots
            OffsetAllocator_Free(allocator, &allocations[243]);
            OffsetAllocator_Free(allocator, &allocations[5]);
            OffsetAllocator_Free(allocator, &allocations[123]);
            OffsetAllocator_Free(allocator, &allocations[95]);

            // Free four contiguous slot (allocator must merge)
            OffsetAllocator_Free(allocator, &allocations[151]);
            OffsetAllocator_Free(allocator, &allocations[152]);
            OffsetAllocator_Free(allocator, &allocations[153]);
            OffsetAllocator_Free(allocator, &allocations[154]);

            OffsetAllocator_Allocate(allocator, 1024 * 1024, &allocations[243]);
            OffsetAllocator_Allocate(allocator, 1024 * 1024, &allocations[5]);
            OffsetAllocator_Allocate(allocator, 1024 * 1024, &allocations[123]);
            OffsetAllocator_Allocate(allocator, 1024 * 1024, &allocations[95]);
            OffsetAllocator_Allocate(allocator, 1024 * 1024 * 4, &allocations[151]); // 4x larger
            REQUIRE(allocations[243].offset != OFFSET_ALLOCATOR_NO_SPACE);
            REQUIRE(allocations[5].offset != OFFSET_ALLOCATOR_NO_SPACE);
            REQUIRE(allocations[123].offset != OFFSET_ALLOCATOR_NO_SPACE);
            REQUIRE(allocations[95].offset != OFFSET_ALLOCATOR_NO_SPACE);
            REQUIRE(allocations[151].offset != OFFSET_ALLOCATOR_NO_SPACE);

            for (uint32_t i = 0; i < 256; i++)
            {
                if (i < 152 || i > 154)
                    OffsetAllocator_Free(allocator, &allocations[i]);
            }
            
            OffsetAllocatorStorageReport report2;
            OffsetAllocator_GetStorageReport(allocator, &report2);
            REQUIRE(report2.totalFreeSpace == 1024 * 1024 * 256);
            REQUIRE(report2.largestFreeRegion == 1024 * 1024 * 256);

            // End: Validate that allocator has no fragmentation left. Should be 100% clean.
            OffsetAllocatorAllocation validateAll;
            OffsetAllocator_Allocate(allocator, MAX_SIZE, &validateAll);
            REQUIRE(validateAll.offset == 0);
            OffsetAllocator_Free(allocator, &validateAll);
        }

        free(buffer);
    }
}

int main(int argc, char* argv[]) 
{
    return Catch::Session().run(argc, argv);
}