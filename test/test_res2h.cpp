#include "checksum.h"
#include "res2h.h"
#include "res2hhelpers.h"
#include "stdfshelpers.h"
#include "syshelpers.h"
#include "test_base.h"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

bool test_roundtrip(stdfs::path dataDir, const stdfs::path &buildDir)
{
#ifdef WIN32
#ifdef _DEBUG
    const stdfs::path res2hPath = "..\\Debug\\res2h.exe";
    const stdfs::path res2hdumpPath = "..\\Debug\\res2hdump.exe";
#else
    const stdfs::path res2hPath = "..\\Release\\res2h.exe";
    const stdfs::path res2hdumpPath = "..\\Release\\res2hdump.exe";
#endif
#else
    const stdfs::path res2hPath = "../src/res2h";
    const stdfs::path res2hdumpPath = "../src/res2hdump";
#endif
    static const std::string res2hdumpOptions = "-v -f"; // dump using full paths
    static const std::string res2hOptions = "-v -r -b"; // recurse and build binary archive

    stdfs::path outDir = stdfs::path("/tmp") / "out";
    stdfs::path outFile = "test.bin";
    std::cout << "res2h integration test " << RES2H_VERSION_STRING << std::endl;
    std::cout << "Reading all files from " << dataDir << std::endl;
    std::cout << "and packing them to " << outDir / outFile << "." << std::endl;
    std::cout << "Then unpacking all files again and comparing binary data." << std::endl;
    // remove all files in results directory
    std::cout << "Deleting and re-creating " << outDir << "." << std::endl;
    try
    {
        stdfs::remove_all(outDir);
    }
    catch (const stdfs::filesystem_error &e)
    {
        // directory was probably not there...
        std::cout << "Warning: " << e.what() << std::endl;
    }
    // and re-create the directory
    stdfs::create_directory(outDir);
    // check if the input/output directories exist now
    try
    {
        dataDir = stdfs::canonical(dataDir);
        outDir = stdfs::canonical(outDir);
    }
    catch (const stdfs::filesystem_error &e)
    {
        // directory was probably not there...
        std::cout << "Error: " << e.what() << std::endl;
        return false;
    }
    // get all files from source directory and generate output directories
    std::vector<FileData> fileList = getFileData(dataDir, dataDir, true);
    for (auto & file : fileList)
    {
        // replace dots in file name with '_' and add a .c/.cpp extension
        std::string newFileName = file.inPath.filename().generic_string();
        auto subPath = naiveRelative(file.inPath, dataDir);
        // add subdir below parent path to name to enable multiple files with the same name
        std::string subDirString(subPath.parent_path().generic_string());
        // build new output file name
        file.outPath = outDir / subDirString / newFileName;
    }
    std::stringstream command;
    // run res2h creating binary archive
    std::cout << "Running res2h to create binary archive..." << std::endl
              << std::endl;
    command << (buildDir / res2hPath) << " " << dataDir << " " << (outDir / outFile) << " " << res2hOptions;
    if (!systemCommand(command.str()))
    {
        // an error occurred running res2h
        std::cout << "The call \"" << command.str() << "\" failed!" << std::endl;
        return false;
    }
    std::stringstream().swap(command);
    // run res2hdump, unpacking all files
    std::cout << "Running res2hdump to unpack binary archive..." << std::endl
              << std::endl;
    command << buildDir / res2hdumpPath << " " << (outDir / outFile) << " " << outDir << " " << res2hdumpOptions;
    if (!systemCommand(command.str()))
    {
        // an error occurred running res2h
        std::cout << "The call \"" << command.str() << "\" failed!" << std::endl;
        return false;
    }
    // compare binary
    std::cout << std::endl
              << "Comparing files..." << std::endl;
    for (auto fdIt = fileList.begin(); fdIt != fileList.cend(); ++fdIt)
    {
        if (!compareFileContent(fdIt->inPath, fdIt->outPath))
        {
            std::cout << "Binary comparison of " << fdIt->inPath << " to " << fdIt->outPath << " failed!" << std::endl;
            return false;
        }
    }
    return true;
}

START_SUITE("Res2h pack/unpack test")
stdfs::path buildDir = stdfs::current_path();
RUN_TEST("Check res2h roundtrip", test_roundtrip(buildDir / "../../test/data/", buildDir))
END_SUITE
