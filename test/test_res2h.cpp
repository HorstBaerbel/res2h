#include "checksum.h"
#include "fshelpers.h"
#include "res2h.h"
#include "syshelpers.h"
#include "test_base.h"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#if defined(__GNUC__) || defined(__clang__)
#include <experimental/filesystem>
namespace stdfs = std::experimental::filesystem;
#elif defined(_MSC_VER)
#include <filesystem>
namespace stdfs = std::tr2::sys;
#endif

struct FileData
{
    stdfs::path inPath;
    stdfs::path outPath;
    std::string internalName;
    std::string dataVariableName;
    std::string sizeVariableName;
    size_t size = 0;
};

std::vector<FileData> getFileDataFrom(const stdfs::path &inPath, const stdfs::path &outPath, const stdfs::path &parentDir, const bool recurse)
{
    // get all files from directory
    std::vector<FileData> files;
    // check for infinite symlinks
    if (stdfs::is_symlink(inPath))
    {
        // check if the symlink points somewhere in the path. this would recurse
        if (inPath.string().find(stdfs::canonical(inPath).string()) == 0)
        {
            std::cout << "Warning: Path " << inPath << " contains recursive symlink! Skipping." << std::endl;
            return files;
        }
    }
    // iterate through source directory searching for files
    const stdfs::directory_iterator dirEnd;
    for (stdfs::directory_iterator fileIt(inPath); fileIt != dirEnd; ++fileIt)
    {
        stdfs::path filePath = (*fileIt).path();
        if (!stdfs::is_directory(filePath))
        {
            // add file to list
            FileData temp;
            temp.inPath = filePath;
            // replace dots in file name with '_' and add a .c/.cpp extension
            std::string newFileName = filePath.filename().generic_string();
            // remove parent directory of file from path for internal name. This could surely be done in a safer way
            stdfs::path subPath(filePath.generic_string().substr(parentDir.generic_string().size() + 1));
            // add a ":/" before the name to mark internal resources (Yes. Hello Qt!)
            temp.internalName = ":/" + subPath.generic_string();
            // add subdir below parent path to name to enable multiple files with the same name
            std::string subDirString(subPath.parent_path().generic_string());
            // build new output file name
            temp.outPath = outPath / subDirString / newFileName;
            // get file size
            try
            {
                temp.size = static_cast<size_t>(stdfs::file_size(filePath));
            }
            catch (...)
            {
                std::cout << "Error: Failed to get size of " << filePath << "!" << std::endl;
                temp.size = 0;
            }
            // add file to list
            files.push_back(temp);
        }
    }
    // does the user want subdirectories?
    if (recurse)
    {
        // iterate through source directory again searching for directories
        for (stdfs::directory_iterator dirIt(inPath); dirIt != dirEnd; ++dirIt)
        {
            stdfs::path dirPath = (*dirIt).path();
            if (stdfs::is_directory(dirPath))
            {
                // subdirectory found. recurse.
                std::vector<FileData> subFiles = getFileDataFrom(dirPath, outPath, parentDir, recurse);
                // add returned result to file list
                files.insert(files.end(), subFiles.cbegin(), subFiles.cend());
            }
        }
    }
    // return result
    return files;
}

bool run(stdfs::path inDir, stdfs::path buildDir)
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
    std::cout << "Reading all files from " << inDir << std::endl;
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
        inDir = stdfs::canonical(inDir);
        outDir = stdfs::canonical(outDir);
    }
    catch (const stdfs::filesystem_error &e)
    {
        // directory was probably not there...
        std::cout << "Error: " << e.what() << std::endl;
        return false;
    }
    // get all files from source directory
    std::vector<FileData> fileList = getFileDataFrom(inDir, outDir, inDir, true);
    std::stringstream command;
    // run res2h creating binary archive
    std::cout << "Running res2h to create binary archive..." << std::endl
              << std::endl;
    command << (buildDir / res2hPath) << " " << inDir << " " << (outDir / outFile) << " " << res2hOptions;
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
RUN_TEST("Check res2h", run(buildDir / "../../test/data/", buildDir))
END_SUITE
