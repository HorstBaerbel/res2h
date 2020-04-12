#include "test_base.h"

#include "fshelpers.h"

static bool test_naiveuncomplete()
{
    CHECK_EQUAL(naiveUncomplete("/foo/bar/new/", "/foo/bar/"),  stdfs::path("new/."))
    CHECK_EQUAL(naiveUncomplete("/foo/bar/new/", "/foo/bar/baz/"),  stdfs::path("../new/."))
    CHECK_EQUAL(naiveUncomplete("/foo/bar/new.file", "/foo/bar/"),  stdfs::path("new.file"))
    CHECK_EQUAL(naiveUncomplete("/foo/bar/new.file", "/foo/bar/baz/"),  stdfs::path("../new.file"))
    CHECK_EQUAL(naiveUncomplete("/foo/bar/new/", "/other/bar/"),  stdfs::path("../../foo/bar/new/."))
    TEST_SUCCEEDED
}

static bool test_normalize()
{
    CHECK_EQUAL(normalize("/foo/../baz"), "/baz")
    CHECK_EQUAL(normalize("/foo/./baz"), "/foo/baz")
    CHECK_EQUAL(normalize("/foo/bar/baz/.././"), "/foo/bar")
    CHECK_EQUAL(normalize("."), stdfs::current_path())
    CHECK_EQUAL(normalize(".."), stdfs::canonical(stdfs::current_path() / ".."))
    TEST_SUCCEEDED
}

static bool test_startsWithPrefix()
{
    CHECK(startsWithPrefix("/foo/bar/baz", "/foo"))
    CHECK(startsWithPrefix("/foo/bar/baz", "/foo/bar"))
    CHECK(!startsWithPrefix("/foo/bar/baz", "/foo/ba"))
    CHECK(!startsWithPrefix("/fuu/bar/baz", "/foo"))
    CHECK(!startsWithPrefix("/fuu/foo/baz", "/foo"))
    CHECK(!startsWithPrefix("/fuu/bar/baz", ""))
    CHECK(!startsWithPrefix("", "/fuu/bar/baz"))
    TEST_SUCCEEDED
}

START_SUITE("File system helper functions")
RUN_TEST("Check naiveUncomplete", test_naiveuncomplete)
RUN_TEST("Check normalize", test_normalize)
RUN_TEST("Check startsWithPrefix", test_startsWithPrefix)
END_SUITE
