#include "test_base.h"

#include "fshelpers.h"

static bool test_naiverelative()
{
    CHECK_EQUAL(naiveRelative("/foo/bar/new.file", "/foo/bar/"),  stdfs::path("new.file"))
    CHECK_EQUAL(naiveRelative("/foo/bar/new.file", "/foo/bar"),  stdfs::path("new.file"))
    CHECK_EQUAL(naiveRelative("/foo/bar/new/", "/foo/bar/"),  stdfs::path("new"))
    CHECK_EQUAL(naiveRelative("/foo/bar/new/", "/foo/bar/baz/"),  stdfs::path("../new"))
    CHECK_EQUAL(naiveRelative("/foo/bar/new.file", "/foo/bar/baz/"),  stdfs::path("../new.file"))
    CHECK_EQUAL(naiveRelative("/foo/bar/new/", "/other/bar/"),  stdfs::path("../../foo/bar/new"))
    CHECK_EQUAL(naiveRelative("/foo/bar/new.file", "/foo/bar/old.file"),  stdfs::path("new.file"))
    CHECK_EQUAL(naiveRelative("/foo/bar/", "/foo/bar/"),  stdfs::path())
    TEST_SUCCEEDED
}

static bool test_naivelexicallynormal()
{
    CHECK_EQUAL(naiveLexicallyNormal("/foo/../baz"), "/baz")
    CHECK_EQUAL(naiveLexicallyNormal("/foo/./baz"), "/foo/baz")
    CHECK_EQUAL(naiveLexicallyNormal("/foo/bar/baz/.././"), "/foo/bar")
    CHECK_EQUAL(naiveLexicallyNormal("/foo/bar/baz/../."), "/foo/bar")
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

static bool test_comparefilecontent(const stdfs::path &dataDir)
{
    CHECK(compareFileContent(dataDir / "test1.png", dataDir / "test1.png"))
    CHECK(!compareFileContent(dataDir / "test1.png",  dataDir / "test2.txt"))
    CHECK_THROW(compareFileContent(dataDir / "not.there", dataDir / "test1.png"), std::runtime_error)
    CHECK_THROW(compareFileContent(dataDir / "test1.png", dataDir / "not.there"), std::runtime_error)
    TEST_SUCCEEDED
}

static bool test_appendfilecontent(const stdfs::path &dataDir)
{
    CHECK(stdfs::copy_file(dataDir / "a.txt", "/tmp/a.txt", stdfs::copy_options::overwrite_existing))
    CHECK(stdfs::copy_file(dataDir / "b.txt", "/tmp/b.txt", stdfs::copy_options::overwrite_existing))
    CHECK_NOTHROW(appendFileContent("/tmp/a.txt", "/tmp/b.txt"))
    CHECK(compareFileContent("/tmp/a.txt", dataDir / "ab.txt"))
    TEST_SUCCEEDED
}

START_SUITE("File system helper functions")
stdfs::path buildDir = stdfs::current_path();
stdfs::path dataDir = buildDir / "../../test/data";
RUN_TEST("Check naiveRelative", test_naiverelative())
RUN_TEST("Check naiveLexicallyNormal", test_naivelexicallynormal())
RUN_TEST("Check startsWithPrefix", test_startsWithPrefix())
RUN_TEST("Check compareFileContent", test_comparefilecontent(dataDir))
RUN_TEST("Check appendFileContent", test_appendfilecontent(dataDir))
END_SUITE
