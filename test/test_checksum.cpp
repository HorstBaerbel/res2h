#include "test_base.h"

#include "checksum.h"

#include <algorithm>
#include <array>
#include <random>

static bool test_fletcher_zero()
{
    std::array<uint8_t, 11> data = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    CHECK_EQUAL(calculateFletcher<uint16_t>(data.data(), 0), 0)
    CHECK_EQUAL(calculateFletcher<uint16_t>(data.data(), data.size()), 0)
    CHECK_EQUAL(calculateFletcher<uint32_t>(data.data(), 0), 0)
    CHECK_EQUAL(calculateFletcher<uint32_t>(data.data(), data.size()), 0)
    CHECK_EQUAL(calculateFletcher<uint64_t>(data.data(), 0), 0)
    CHECK_EQUAL(calculateFletcher<uint64_t>(data.data(), data.size()), 0)
    TEST_SUCCEEDED
}

static bool test_fletcher_difflengths()
{
    std::array<uint8_t, 11> data = {5, 4, 123, 3, 12, 200, 2, 11};
    CHECK_EQUAL(calculateFletcher<uint16_t>(data.data(), 1), 1285)
    CHECK_EQUAL(calculateFletcher<uint16_t>(data.data(), 2), 3593)
    CHECK_EQUAL(calculateFletcher<uint32_t>(data.data(), 1), 327685)
    CHECK_EQUAL(calculateFletcher<uint32_t>(data.data(), 2), 67437573)
    CHECK_EQUAL(calculateFletcher<uint32_t>(data.data(), 3), 142935168)
    CHECK_EQUAL(calculateFletcher<uint32_t>(data.data(), 4), 193267584)
    CHECK_EQUAL(calculateFletcher<uint64_t>(data.data(), 1), 21474836485)
    CHECK_EQUAL(calculateFletcher<uint64_t>(data.data(), 2), 4419521348613)
    CHECK_EQUAL(calculateFletcher<uint64_t>(data.data(), 3), 34625841664820229)
    CHECK_EQUAL(calculateFletcher<uint64_t>(data.data(), 4), 250798623828935685)
    CHECK_EQUAL(calculateFletcher<uint64_t>(data.data(), 5), 501597299139085329)
    CHECK_EQUAL(calculateFletcher<uint64_t>(data.data(), 6), 501817201464691729)
    CHECK_EQUAL(calculateFletcher<uint64_t>(data.data(), 7), 502380151418244113)
    CHECK_EQUAL(calculateFletcher<uint64_t>(data.data(), 8), 1295013686020000785)
    TEST_SUCCEEDED
}

static bool test_fletcher_result()
{
    std::array<uint8_t, 11> data = {5, 4, 123, 3, 12, 200, 0, 11, 61, 12, 101};
    CHECK_EQUAL(calculateFletcher<uint16_t>(data.data(), data.size()), 11796)
    CHECK_EQUAL(calculateFletcher<uint32_t>(data.data(), data.size()), 2207573806)
    CHECK_EQUAL(calculateFletcher<uint64_t>(data.data(), data.size()), 2366545276906297422)
    TEST_SUCCEEDED
}

static bool test_fletcher_sameresult()
{
    std::array<uint8_t, 256> data{};
    std::random_device rd;
    std::mt19937 mte(rd());
    std::uniform_int_distribution<uint8_t> dist(0, 255);
    std::generate(data.begin(), data.end(), [&](){ return dist(mte); });
    CHECK_EQUAL(calculateFletcher<uint16_t>(data.data(), data.size()), calculateFletcher<uint16_t>(data.data(), data.size()))
    CHECK_EQUAL(calculateFletcher<uint32_t>(data.data(), data.size()), calculateFletcher<uint32_t>(data.data(), data.size()))
    CHECK_EQUAL(calculateFletcher<uint64_t>(data.data(), data.size()), calculateFletcher<uint64_t>(data.data(), data.size()))
    TEST_SUCCEEDED
}

START_SUITE("Checksum functions")
RUN_TEST("Fletcher results", test_fletcher_result())
RUN_TEST("Fletcher all zeros", test_fletcher_zero())
RUN_TEST("Fletcher different lengths", test_fletcher_difflengths())
RUN_TEST("Fletcher gives consistent results", test_fletcher_sameresult())
END_SUITE
