#include "test_base.h"

#include "fshelpers.h"

static bool test_naiverelative()
{
    CHECK_EQUAL(naiveRelative("/foo/bar/", "/foo/bar/"),  stdfs::path("."))
    CHECK_EQUAL(naiveRelative("/foo/bar/new/", "/foo/bar/"),  stdfs::path("new/."))
    CHECK_EQUAL(naiveRelative("/foo/bar/new/", "/foo/bar/baz/"),  stdfs::path("../new/."))
    CHECK_EQUAL(naiveRelative("/foo/bar/new.file", "/foo/bar/"),  stdfs::path("new.file"))
    CHECK_EQUAL(naiveRelative("/foo/bar/new.file", "/foo/bar/baz/"),  stdfs::path("../new.file"))
    CHECK_EQUAL(naiveRelative("/foo/bar/new/", "/other/bar/"),  stdfs::path("../../foo/bar/new/."))
    CHECK_EQUAL(naiveRelative("/foo/bar/new.file", "/foo/bar/old.file"),  stdfs::path("new.file"))
    TEST_SUCCEEDED
}

static bool test_naivelexicallynormal()
{
    CHECK_EQUAL(naiveLexicallyNormal("/foo/../baz"), "/baz")
    CHECK_EQUAL(naiveLexicallyNormal("/foo/./baz"), "/foo/baz")
    CHECK_EQUAL(naiveLexicallyNormal("/foo/bar/baz/.././"), "/foo/bar")
    CHECK_EQUAL(naiveLexicallyNormal("."), stdfs::current_path())
    CHECK_EQUAL(naiveLexicallyNormal(".."), stdfs::canonical(stdfs::current_path() / ".."))
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
RUN_TEST("Check naiveRelative", test_naiverelative)
RUN_TEST("Check naiveLexicallyNormal", test_naivelexicallynormal)
RUN_TEST("Check startsWithPrefix", test_startsWithPrefix)
END_SUITE
