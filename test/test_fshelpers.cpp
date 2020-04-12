#include "test_base.h"

#include "fshelpers.h"

static bool test_hasprefix()
{
    CHECK(hasPrefix("/foo/bar/baz", "/foo"))
    CHECK(!hasPrefix("/fuu/bar/baz", "/foo"))
    TEST_SUCCEEDED
}

START_SUITE("File system helper functions")
RUN_TEST("hasPrefix", test_hasprefix)
END_SUITE
