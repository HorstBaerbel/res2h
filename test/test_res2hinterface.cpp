#include "checksum.h"
#include "res2hinterface.h"
#include "stdfs.h"
#include "syshelpers.h"
#include "test_base.h"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

static const Res2h::ArchiveInfo ReferenceArchive = {"/tmp/test.bin", 0, 2, 32, 32, 19485, 0x5cc5a806};
static const std::vector<Res2h::ResourceInfo> ReferenceResource = {
    {":/a.txt", {}, 4, 270, 0xcd236bc2},
    {":/ab.txt", {}, 7, 274, 0x6975ce2e},
    {":/b.txt", {}, 3, 281, 0xc4ce626c},
    {":/subdir/a.txt", {}, 4, 284, 0xcd236bc2},
    {":/subdir/subdir2/test3.txt", {}, 48, 288, 0x6bd61659},
    {":/subdir/test2.jpg", {}, 5459, 336, 0x46d7bec9},
    {":/test1.png", {}, 13095, 5795, 0x741b0dba},
    {":/test2.txt", {}, 591, 18890, 0x31c068ce}
};

bool test_archivecontent(const stdfs::path &dataDir, const stdfs::path &buildDir)
{
#ifdef WIN32
#ifdef _DEBUG
    const stdfs::path res2hPath = "..\\Debug\\res2h.exe";
#else
    const stdfs::path res2hPath = "..\\Release\\res2h.exe";
#endif
#else
    const stdfs::path res2hPath = "../src/res2h";
#endif
    static const std::string res2hOptions = "-v -r -b"; // recurse and build binary archive

    stdfs::path outFile = stdfs::path("/tmp") / "test.bin";
    std::cout << "res2h integration test " << RES2H_VERSION_STRING << std::endl;
    std::cout << "Reading all files from " << dataDir << std::endl;
    std::cout << "and packing them to " << outFile << "." << std::endl;
    std::cout << "Then using res2interface to access the archive." << std::endl;
    std::stringstream command;
    // run res2h creating binary archive
    std::cout << "Running res2h to create binary archive..." << std::endl
              << std::endl;
    command << (buildDir / res2hPath) << " " << dataDir << " " << outFile << " " << res2hOptions;
    if (!systemCommand(command.str()))
    {
        // an error occurred running res2h
        std::cout << "The call \"" << command.str() << "\" failed!" << std::endl;
        return false;
    }
    // use res2hinterface to get archive content
    auto res2h = Res2h::instance();
    // compare archive info
    Res2h::ArchiveInfo archive;
    CHECK_NOTHROW(archive = res2h.archiveInfo(outFile))
    CHECK_EQUAL(archive, ReferenceArchive)
    // load archive and get resource information
    CHECK(res2h.loadArchive(outFile))
    std::vector<std::reference_wrapper<const Res2h::ResourceInfo>> resources;
    CHECK_NOTHROW(resources = res2h.resourceInfo())
    CHECK_EQUAL(resources.size(), ReferenceResource.size())
    for (decltype(resources.size()) i = 0; i < resources.size(); ++i)
    {
        auto resource = resources.at(i).get();
        if (resource != ReferenceResource.at(i))
        {
            std::cout << "File \"" << resource.filePath << "\" not found" << std::endl;
            return false;
        }
    }
    return true;
}

START_SUITE("Res2hinterface test")
stdfs::path buildDir = stdfs::current_path();
RUN_TEST("Check archive content", test_archivecontent(buildDir / "../../test/data/", buildDir))
END_SUITE
