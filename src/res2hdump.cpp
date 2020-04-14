#include "checksum.h"
#include "res2hinterface.h"
#include "stdfs.h"
#include "stdfshelpers.h"

#include <fstream>
#include <iostream>

static bool beVerbose = false;
static bool useFullPaths = false;
static bool informationOnly = false;
static stdfs::path inFilePath;
static stdfs::path outFilePath;

static void printVersion()
{
    std::cout << "res2hdump " << RES2H_VERSION_STRING << " - Dump data from a res2h archive file or embedded archive." << std::endl
              << std::endl;
}

static void printUsage()
{
    std::cout << std::endl;
    std::cout << "Usage: res2hdump <archive> [<outdir>] [options]" << std::endl;
    std::cout << "Valid options:" << std::endl;
    std::cout << "-f Recreate path structure, creating directories as needed." << std::endl;
    std::cout << "-i Display information about the archive and files, but don't extract anything." << std::endl;
    std::cout << "-v Be verbose." << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "res2hdump ./resources/data.bin -i (display archive information)" << std::endl;
    std::cout << "res2hdump ./resources/data.bin ./resources (extract files from archive)" << std::endl;
    std::cout << "res2hdump ./resources/program.exe ./resources (extract files from embedded archive)" << std::endl;
}

static bool readArguments(const std::vector<std::string> &arguments)
{
    bool pastFiles = false;
    for (const auto &argument : arguments)
    {
        // check what it is
        if (argument == "-f")
        {
            useFullPaths = true;
            pastFiles = true;
        }
        else if (argument == "-i")
        {
            informationOnly = true;
            pastFiles = true;
        }
        else if (argument == "-v")
        {
            beVerbose = true;
            pastFiles = true;
        }
        // none of the options was matched until here...
        else if (!pastFiles)
        {
            // if no files/directories have been found yet this is probably a file/directory
            if (inFilePath.empty())
            {
                inFilePath = naiveLexicallyNormal(stdfs::path(argument));
                if (inFilePath.empty())
                {
                    return false;
                }
            }
            else if (outFilePath.empty())
            {
                outFilePath = naiveLexicallyNormal(stdfs::path(argument));
                if (outFilePath.empty())
                {
                    return false;
                }
                pastFiles = true;
            }
        }
        else
        {
            std::cerr << "Error: Unknown argument \"" << argument << "\"!" << std::endl;
            return false;
        }
    }
    return true;
}

// -----------------------------------------------------------------------------

static bool dumpArchive(const stdfs::path &destination, const stdfs::path &archive, bool createPaths, bool extract)
{
    try
    {
        if (Res2h::instance().loadArchive(archive.string()))
        {
            // dump archive information
            const auto archiveInfo = Res2h::instance().archiveInfo(archive.string());
            std::cout << "Archive file: \"" << archiveInfo.filePath << std::endl;
            std::cout << "Data offset: " << std::dec << archiveInfo.offsetInFile << " bytes" << std::endl;
            std::cout << "Size: " << std::dec << archiveInfo.size << " bytes" << std::endl;
            std::cout << "File version: " << std::dec << archiveInfo.fileVersion << std::endl;
            std::cout << "File format: " << std::hex << std::showbase << archiveInfo.formatFlags << std::endl;
            std::cout << "Bits: " << std::dec << static_cast<uint32_t>(archiveInfo.bits) << std::endl;
            std::cout << "Checksum: " << std::hex << std::showbase << archiveInfo.checksum << std::endl;
            std::cout << "------------------------------------------------------------------------" << std::endl;
            // dump resource information
            const auto resources = Res2h::instance().resourceInfo();
            for (uint32_t i = 0; i < resources.size(); ++i)
            {
                // read resource entry
                const auto &entry = resources.at(i).get();
                // dump to console
                std::cout << "File #" << std::dec << i << " \"" << entry.filePath << "\"" << std::endl;
                std::cout << "Data offset: " << std::dec << entry.dataOffset << " bytes" << std::endl;
                std::cout << "Data size: " << std::dec << entry.dataSize << " bytes" << std::endl;
                std::cout << "Checksum: " << std::hex << std::showbase << entry.checksum << std::endl;
                if (extract)
                {
                    // if the caller wants to dump data, do it
                    try
                    {
                        auto file = Res2h::instance().loadResource(entry.filePath);
                        if (!file.data.empty() && file.dataSize > 0)
                        {
                            // worked. now dump file data to disk. construct output path
                            std::string filePath = entry.filePath;
                            stdfs::path subPath = filePath.erase(0, 2);
                            stdfs::path outPath = destination / subPath;
                            if (createPaths)
                            {
                                stdfs::path dirPath = destination;
                                for (auto sdIt = subPath.begin(); sdIt->filename() != subPath.filename(); ++sdIt)
                                {
                                    // build output path with subdirectory
                                    dirPath /= *sdIt;
                                    // check if if exists
                                    if (!stdfs::exists(dirPath))
                                    {
                                        stdfs::create_directory(dirPath);
                                    }
                                }
                            }
                            // try to open output file
                            std::ofstream outStream;
                            outStream.open(outPath.string(), std::ios_base::out | std::ios_base::trunc | std::ios_base::binary);
                            if (!outStream.is_open() || !outStream.good())
                            {
                                std::cerr << "Failed to open file for writing" << std::endl;
                                continue;
                            }
                            // write data to disk and check if all data has been written
                            outStream.write(reinterpret_cast<const char *>(file.data.data()), static_cast<std::streamsize>(file.dataSize));
                            if (static_cast<uint64_t>(outStream.tellp()) != file.dataSize)
                            {
                                std::cerr << "Failed to write all data for resource #" << std::dec << i << std::endl;
                            }
                            // close file
                            outStream.close();
                        }
                    }
                    catch (const Res2hException &e)
                    {
                        std::cerr << "Error loading resource " << std::dec << i << " from archive - " << e.what() << std::endl;
                    }
                }
            }
            return true;
        }
        std::cerr << "Failed to open archive " << archive << std::endl;
    }
    catch (const Res2hException &e)
    {
        std::cerr << "Error dumping archive: " << e.what() << std::endl;
    }
    return false;
}

int main(int argc, const char *argv[])
{
    printVersion();
    // copy all arguments except program name to vector
    std::vector<std::string> arguments;
    for (int i = 1; i < argc; ++i)
    {
        arguments.emplace_back(std::string(argv[i]));
    }
    // check number of arguments and if all arguments can be read
    if (argc < 2 || !readArguments(arguments))
    {
        printUsage();
        return -1;
    }
    // check if the input path exist
    if (!stdfs::exists(inFilePath))
    {
        std::cerr << "Error: Invalid input file \"" << inFilePath.string() << "\"!" << std::endl;
        return -2;
    }
    // check if argument 1 is a file
    if (stdfs::is_directory(inFilePath))
    {
        std::cerr << "Error: Input must be a file!" << std::endl;
        return -2;
    }
    if (!informationOnly)
    {
        // check if argument 2 is a directory
        if (!stdfs::is_directory(outFilePath))
        {
            std::cerr << "Error: Output must be a directory!" << std::endl;
            return -2;
        }
    }
    if (!dumpArchive(outFilePath, inFilePath, useFullPaths, !informationOnly))
    {
        std::cerr << "Failed to dump archive!" << std::endl;
        return -3;
    }
    // profit!!!
    std::cout << "res2hdump succeeded." << std::endl;
    return 0;
}
