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

static const Res2h::ArchiveInfo ReferenceArchive = {"/tmp/test.bin", 0, 2, 32, 32, 19485, 0xb858a65c};
static const std::vector<Res2h::ResourceInfo> ReferenceResource = {
    {":/ab.txt", {}, 7, 270, 0x6975ce2e},
    {":/a.txt", {}, 4, 277, 0xcd236bc2},
    {":/test1.png", {}, 13095, 281, 0x741b0dba},
    {":/b.txt", {}, 3, 13376, 0xc4ce626c},
    {":/test2.txt", {}, 591, 13379, 0x31c068ce},
    {":/subdir/a.txt", {}, 4, 13970, 0xcd236bc2},
    {":/subdir/test2.jpg", {}, 5459, 13974, 0x46d7bec9},
    {":/subdir/subdir2/test3.txt", {}, 48, 19433, 0x6bd61659}
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
    for (const auto & resRef : resources)
    {
        // try to find file in reference. we don't care about the order of the files
        auto & resource = resRef.get();
        if (std::find(ReferenceResource.cbegin(), ReferenceResource.cend(), resource) == ReferenceResource.cend())
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
