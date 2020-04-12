#include "test_base.h"

#include "checksum.h"

int bla()
{
    return 0;
}

static bool test_fletcher_zero()
{
    std::array<uint8_t, 11> data = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    CHECK_EQUAL(bla(), 0);
    CHECK_EQUAL(calculateFletcher<uint16_t>(data.data(), data.size()), 0);
    CHECK_EQUAL(calculateFletcher<uint32_t>(data.data(), data.size()), 0);
    CHECK_EQUAL(calculateFletcher<uint64_t>(data.data(), data.size()), 0);
    TEST_SUCCEEDED
}

START_SUITE("Checksum functions")
RUN_TEST("Fletcher all zeros", test_fletcher_zero)
END_SUITE
