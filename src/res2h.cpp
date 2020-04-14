#include "res2h.h"
#include "checksum.h"
#include "fshelpers.h"
#include "syshelpers.h"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
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
    uint64_t size = 0;
};

static bool beVerbose = false;
static bool useRecursion = false;
static bool useC = false;
static bool createBinary = false;
static bool appendFile = false;
static bool combineResults = false;
static stdfs::path commonHeaderFilePath;
static stdfs::path utilitiesFilePath;
static stdfs::path inFilePath;
static stdfs::path outFilePath;
static std::ofstream badOfStream; // we need this later as a default parameter...
static const std::string indent = "    ";

/// @brief Macro to do / print something if verbose output is on. Yes, this should probably be a template...
#define IF_BEVERBOSE(a)         \
    {                           \
        if (beVerbose) { (a); } \
    }

static void printVersion()
{
    std::cout << "res2h " << RES2H_VERSION_STRING << " - Load plain binary data and dump to a raw C/C++ array." << std::endl
              << std::endl;
}

static void printUsage()
{
    std::cout << "Usage: res2h INFILE/INDIR OUTFILE/OUTDIR [OPTIONS]" << std::endl;
    std::cout << "Valid OPTIONS:" << std::endl;
    std::cout << "-r Recurse into subdirectories below indir." << std::endl;
    std::cout << "-c Use .c files and C-arrays for storing the data definitions, else" << std::endl;
    std::cout << "   uses .cpp files and std::vector/std::map." << std::endl;
    std::cout << "-h HEADERFILE Puts all declarations in a common \"HEADERFILE\" using \"extern\"" << std::endl;
    std::cout << "   and includes that header file in the source files." << std::endl;
    std::cout << "-u SOURCEFILE Create utility functions and arrays in a .c/.cpp file." << std::endl;
    std::cout << "   Only makes sense in combination with -h" << std::endl;
    std::cout << "-1 Combine all converted files into one big .c/.cpp file (use with -u)." << std::endl;
    std::cout << "-b Compile binary archive outfile containing all infile(s). For reading in your" << std::endl;
    std::cout << "   software include res2hinterface.h/.cpp and consult the docs." << std::endl;
    std::cout << "-a Append infile to outfile. Can be used to append an archive to an executable." << std::endl;
    std::cout << "-v Be verbose." << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "res2h ./lenna.png ./resources/lenna_png.cpp (convert single file)" << std::endl;
    std::cout << "res2h ./data ./resources -s -h resources.h -u resources.cpp (convert directory)" << std::endl;
    std::cout << "res2h ./data ./resources/data.bin -b (convert directory to binary file)" << std::endl;
    std::cout << "res2h ./resources/data.bin ./program.exe -a (append archive to executable)" << std::endl;
}

static bool readArguments(const std::vector<std::string> &arguments)
{
    bool pastFiles = false;
    for (auto aIt = arguments.cbegin(); aIt != arguments.cend(); ++aIt)
    {
        // check what it is
        auto argument = *aIt;
        if (argument == "-a")
        {
            if (!commonHeaderFilePath.empty() || !utilitiesFilePath.empty())
            {
                std::cerr << "Option -a can not be combined with -h or -u" << std::endl;
                return false;
            }
            if (createBinary)
            {
                std::cerr << "Option -a can not be combined with -b" << std::endl;
                return false;
            }
            if (combineResults)
            {
                std::cerr << "Option -a can not be combined with -1" << std::endl;
                return false;
            }
            appendFile = true;
            pastFiles = true;
        }
        else if (argument == "-1")
        {
            // -u must be used for this to work. check if specified
            if (std::find(arguments.cbegin(), arguments.cend(), "-u") != arguments.cend())
            {
                combineResults = true;
                pastFiles = true;
                break;
            }
            if (!combineResults)
            {
                // -u not specified. complain to user.
                std::cerr << "Option -1 has to be combined with -u" << std::endl;
                return false;
            }
        }
        else if (argument == "-b")
        {
            if (!commonHeaderFilePath.empty() || !utilitiesFilePath.empty())
            {
                std::cerr << "Option -b can not be combined with -h or -u" << std::endl;
                return false;
            }
            if (appendFile)
            {
                std::cerr << "Option -b can not be combined with -a" << std::endl;
                return false;
            }
            if (combineResults)
            {
                std::cerr << "Warning: Creating binary archive. Option -1 ignored" << std::endl;
                return false;
            }
            createBinary = true;
            pastFiles = true;
        }
        else if (argument == "-c")
        {
            useC = true;
            pastFiles = true;
        }
        else if (argument == "-r")
        {
            useRecursion = true;
            pastFiles = true;
        }
        else if (argument == "-v")
        {
            beVerbose = true;
            pastFiles = true;
        }
        else if (argument == "-h")
        {
            if (createBinary)
            {
                std::cerr << "Option -h can not be combined with -b" << std::endl;
                return false;
            }
            if (appendFile)
            {
                std::cerr << "Option -h can not be combined with -a" << std::endl;
                return false;
            }
            // try getting next argument as header file name
            if (++aIt != arguments.cend())
            {
                commonHeaderFilePath = naiveLexicallyNormal(stdfs::path(*aIt));
                if (commonHeaderFilePath.empty())
                {
                    return false;
                }
            }
            else
            {
                std::cerr << "Option -h specified, but no file name found" << std::endl;
                return false;
            }
            pastFiles = true;
        }
        else if (argument == "-u")
        {
            if (createBinary)
            {
                std::cerr << "Option -u can not be combined with -b" << std::endl;
                return false;
            }
            if (appendFile)
            {
                std::cerr << "Option -u can not be combined with -a" << std::endl;
                return false;
            }
            // try getting next argument as utility file name++
            if (++aIt != arguments.cend())
            {
                utilitiesFilePath = naiveLexicallyNormal(stdfs::path(*aIt));
                if (utilitiesFilePath.empty())
                {
                    return false;
                }
            }
            else
            {
                std::cerr << "Option -u specified, but no file name found" << std::endl;
                return false;
            }
            if (!utilitiesFilePath.empty() && commonHeaderFilePath.empty())
            {
                std::cerr << "Warning: -u does not make much sense without -h..." << std::endl;
            }
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
            std::cerr << "Unknown argument \"" << argument << "\"" << std::endl;
            return false;
        }
    }
    return true;
}

static std::vector<FileData> getFileDataFrom(const stdfs::path &inPath, const stdfs::path &outPath, const stdfs::path &parentDir, bool recurse)
{
    // get all files from directory
    std::vector<FileData> files;
    // check for infinite symlinks
    if (hasRecursiveSymlink(inPath))
    {
        std::cerr << "Warning: Path " << inPath << " contains recursive symlink! Skipping." << std::endl;
        return files;
    }
    // iterate through source directory searching for files
    const stdfs::directory_iterator dirEnd;
    for (stdfs::directory_iterator fileIt(inPath); fileIt != dirEnd; ++fileIt)
    {
        stdfs::path filePath = (*fileIt).path();
        if (stdfs::is_regular_file(filePath))
        {
            IF_BEVERBOSE(std::cout << "Found input file " << filePath << std::endl)
            FileData temp;
            temp.inPath = filePath;
            // replace dots in file name with '_' and add a .c/.cpp extension
            std::string newFileName = filePath.filename().generic_string();
            std::replace(newFileName.begin(), newFileName.end(), '.', '_');
            if (useC)
            {
                newFileName.append(".c");
            }
            else
            {
                newFileName.append(".cpp");
            }
            // remove parent directory of file from path for internal name
            IF_BEVERBOSE(std::cout << "File path: " << filePath << std::endl)
            IF_BEVERBOSE(std::cout << "Parent dir: " << parentDir << std::endl)
            auto subPath = naiveRelative(filePath, parentDir);
            // add a ":/" before the name to mark internal resources (Yes. Hello Qt!)
            temp.internalName = ":/" + subPath.generic_string();
            // add subdir below parent path to name to enable multiple files with the same name
            std::string subDirString(subPath.remove_filename().generic_string());
            if (!subDirString.empty())
            {
                // replace dir separators by underscores and add in front of file name
                std::replace(subDirString.begin(), subDirString.end(), '/', '_');
                subDirString += "_";
                subDirString += newFileName;
                newFileName = subDirString;
            }
            // build new output file name
            temp.outPath = outPath / newFileName;
            IF_BEVERBOSE(std::cout << "Internal name will be \"" << temp.internalName << "\"" << std::endl)
            IF_BEVERBOSE(std::cout << "Output path is " << temp.outPath << std::endl)
            try
            {
                // get file size and add to file list
                temp.size = static_cast<uint64_t>(stdfs::file_size(filePath));
                IF_BEVERBOSE(std::cout << "Size is " << temp.size << " bytes." << std::endl)
                files.push_back(temp);
            }
            catch (const stdfs::filesystem_error &e)
            {
                std::cerr << "Failed to get size of " << filePath << ": " << e.what() << std::endl;
                std::cerr << "Skipping file" << std::endl;
            }
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
                IF_BEVERBOSE(std::cout << "Found subdirectory " << dirPath << std::endl)
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

static bool convertFile(FileData &fileData, const stdfs::path &commonHeaderPath, std::ofstream &outStream = badOfStream, bool addHeader = true)
{
    if (!stdfs::exists(fileData.inPath))
    {
        std::cerr << "File \"" << fileData.inPath.string() << "\" does not exist" << std::endl;
        return false;
    }
    // try to open the input file
    std::ifstream inStream;
    inStream.open(fileData.inPath.string(), std::ios_base::in | std::ios_base::binary);
    if (!inStream.is_open() || !inStream.good())
    {
        std::cerr << "Failed to open file \"" << fileData.inPath.string() << "\" for reading" << std::endl;
        return false;
    }
    IF_BEVERBOSE(std::cout << "Converting input file " << fileData.inPath)
    // try getting size of data
    inStream.seekg(0, std::ios::end);
    fileData.size = static_cast<uint64_t>(inStream.tellg());
    inStream.seekg(0);
    // check if the caller passed an output stream and use that
    bool closeOutStream = false;
    if (!outStream.is_open() || !outStream.good())
    {
        if (!fileData.outPath.empty())
        {
            // try opening the output stream. truncate it when it exists
            outStream.open(fileData.outPath.string(), std::ofstream::out | std::ofstream::trunc);
        }
        else
        {
            std::cerr << "No output stream passed, but output path for \"" << fileData.inPath.filename().string() << "\" is empty! Skipping." << std::endl;
            return false;
        }
        closeOutStream = true;
    }
    if (!outStream.is_open() || !outStream.good())
    {
        std::cerr << "Failed to open file \"" << fileData.outPath.string() << "\" for writing" << std::endl;
        return false;
    }
    // check if caller wants to add a header
    if (addHeader)
    {
        // add message
        outStream << "// this file was auto-generated from \"" << fileData.inPath.filename().string() << "\" by res2h at " << currentDateAndTime() << std::endl
                  << std::endl;
        // add header include
        if (!commonHeaderPath.empty())
        {
            // common header path must be relative to destination directory
            stdfs::path relativeHeaderPath = naiveRelative(commonHeaderPath, fileData.outPath);
            outStream << "#include \"" << relativeHeaderPath.generic_string() << "\"" << std::endl
                      << std::endl;
        }
    }
    // create names for variables
    fileData.dataVariableName = fileData.outPath.filename().stem().string() + "_data";
    fileData.sizeVariableName = fileData.outPath.filename().stem().string() + "_size";
    // add size and data variable
    if (fileData.size <= UINT16_MAX)
    {
        outStream << "const uint16_t ";
    }
    else if (fileData.size <= UINT32_MAX)
    {
        outStream << "const uint32_t ";
    }
    else
    {
        outStream << "const uint64_t ";
    }
    outStream << fileData.sizeVariableName << " = " << std::dec << fileData.size << ";" << std::endl;
    outStream << "const uint8_t " << fileData.dataVariableName << "[" << std::dec << fileData.size << "] = {" << std::endl;
    outStream << indent; // first indent
    // now add content
    uint64_t breakCounter = 0;
    while (!inStream.eof())
    {
        // read byte from source
        unsigned char dataByte;
        inStream.read(reinterpret_cast<char *>(&dataByte), 1);
        // check if we have actually read something
        if (inStream.gcount() != 1 || inStream.eof())
        {
            // we failed to read. break the read loop and close the file.
            break;
        }
        // write to destination in hex with a width of 2 and '0' as padding. we do not use showbase, as it doesn't work with zero values
        outStream << "0x" << std::setw(2) << std::setfill('0') << std::hex << static_cast<unsigned int>(dataByte);
        // was this the last character?
        if (!inStream.eof() && fileData.size > static_cast<uint64_t>(inStream.tellg()))
        {
            // no. add comma.
            outStream << ",";
            // add break after 14 bytes and add indent again
            if (++breakCounter % 14 == 0)
            {
                outStream << std::endl
                          << indent;
            }
        }
    }
    // add closing curly braces
    outStream << std::endl
              << "};" << std::endl
              << std::endl;
    // close files
    if (closeOutStream)
    {
        outStream.close();
    }
    inStream.close();
    IF_BEVERBOSE(std::cout << " - succeeded." << std::endl)
    return true;
}

static bool createCommonHeader(const std::vector<FileData> &fileList, const stdfs::path &commonHeaderPath, bool addUtilityFunctions, bool useCConstructs)
{
    // try opening the output file. truncate it when it exists
    std::ofstream outStream;
    outStream.open(commonHeaderPath.generic_string(), std::ofstream::out | std::ofstream::trunc);
    if (!outStream.is_open() || !outStream.good())
    {
        std::cerr << "Failed to open file \"" << commonHeaderPath << "\" for writing" << std::endl;
        return false;
    }
    IF_BEVERBOSE(std::cout << std::endl
                           << "Creating common header " << commonHeaderPath)
    // add message
    outStream << "// this file was auto-generated by res2h at " << currentDateAndTime() << std::endl
              << std::endl;
    // add #pragma to only include once
    outStream << "#pragma once" << std::endl
              << std::endl;
    // add includes for C++
    if (!useCConstructs)
    {
        outStream << "#include <string>" << std::endl;
        if (addUtilityFunctions)
        {
            outStream << "#include <map>" << std::endl;
        }
        outStream << std::endl;
    }
    // add all files and check maximum size
    uint64_t maxSize = 0;
    for (const auto &fdIt : fileList)
    {
        // add size and data variable
        maxSize = maxSize < fdIt.size ? fdIt.size : maxSize;
        if (fdIt.size <= UINT16_MAX)
        {
            outStream << "extern const uint16_t ";
        }
        else if (fdIt.size <= UINT32_MAX)
        {
            outStream << "extern const uint32_t ";
        }
        else
        {
            outStream << "extern const uint64_t ";
        }
        outStream << fdIt.sizeVariableName << ";" << std::endl;
        outStream << "extern const uint8_t " << fdIt.dataVariableName << "[];" << std::endl
                  << std::endl;
    }
    // if we want utilities, add array
    if (addUtilityFunctions)
    {
        // add resource struct
        outStream << "struct Res2hEntry {" << std::endl;
        if (useCConstructs)
        {
            outStream << indent << "const char * relativeFileName;" << std::endl;
        }
        else
        {
            outStream << indent << "const std::string relativeFileName;" << std::endl;
        }
        //  add size member depending on the determined maximum file size
        if (maxSize <= UINT16_MAX)
        {
            outStream << indent << "const uint16_t size;" << std::endl;
        }
        else if (maxSize <= UINT32_MAX)
        {
            outStream << indent << "const uint32_t size;" << std::endl;
        }
        else
        {
            outStream << indent << "const uint64_t size;" << std::endl;
        }
        outStream << indent << "const uint8_t * data;" << std::endl;
        outStream << "};" << std::endl
                  << std::endl;
        // add list holding files
        outStream << "extern const uint32_t res2hNrOfFiles;" << std::endl;
        outStream << "extern const Res2hEntry res2hFiles[];" << std::endl
                  << std::endl;
        if (!useCConstructs)
        {
            // add additional std::map if C++
            outStream << "typedef const std::map<const std::string, const Res2hEntry> res2hMapType;" << std::endl;
            outStream << "extern res2hMapType res2hMap;" << std::endl;
        }
    }
    // close file
    outStream.close();
    IF_BEVERBOSE(std::cout << " - succeeded." << std::endl)
    return true;
}

static bool createUtilities(std::vector<FileData> &fileList, const stdfs::path &utilitiesPath, const stdfs::path &commonHeaderPath, bool useCConstructs, bool addFileData)
{
    // try opening the output file. truncate it when it exists
    std::ofstream outStream;
    outStream.open(utilitiesPath.generic_string(), std::ofstream::out | std::ofstream::trunc);
    if (!outStream.is_open() || !outStream.good())
    {
        std::cerr << "Failed to open file \"" << utilitiesPath << "\" for writing" << std::endl;
        return false;
    }
    IF_BEVERBOSE(std::cout << std::endl
                           << "Creating utilities file " << utilitiesPath)
    // add message
    outStream << "// this file was auto-generated by res2h at " << currentDateAndTime() << std::endl
              << std::endl;
    // create path to include file RELATIVE to this file
    stdfs::path relativePath = naiveRelative(commonHeaderPath, utilitiesPath);
    // include header file
    outStream << "#include \"" << relativePath.string() << "\"" << std::endl
              << std::endl;
    // if the data should go to this file too, add it
    if (addFileData)
    {
        for (auto &fd : fileList)
        {
            if (!convertFile(fd, commonHeaderFilePath, outStream, false))
            {
                std::cerr << "Failed to convert all files. Aborting" << std::endl;
                outStream.close();
                return false;
            }
        }
    }
    // begin data arrays. switch depending whether C or C++
    outStream << "const uint32_t res2hNrOfFiles = " << fileList.size() << ";" << std::endl;
    // add files
    outStream << "const Res2hEntry res2hFiles[res2hNrOfFiles] = {" << std::endl;
    outStream << indent; // first indent
    for (auto fdIt = fileList.cbegin(); fdIt != fileList.cend();)
    {
        outStream << "{\"" << fdIt->internalName << "\", " << fdIt->sizeVariableName << ", " << fdIt->dataVariableName << "}";
        // was this the last entry?
        ++fdIt;
        if (fdIt != fileList.cend())
        {
            // no. add comma.
            outStream << ",";
            // add break after every entry and add indent again
            outStream << std::endl
                      << indent;
        }
    }
    outStream << std::endl
              << "};" << std::endl;
    if (!useCConstructs)
    {
        // add files to map
        outStream << std::endl
                  << "res2hMapType::value_type mapTemp[] = {" << std::endl;
        outStream << indent;
        for (auto fdIt = fileList.cbegin(); fdIt != fileList.cend();)
        {
            outStream << "std::make_pair(\"" << fdIt->internalName << "\", res2hFiles[" << (fdIt - fileList.cbegin()) << "])";
            // was this the last entry?
            ++fdIt;
            if (fdIt != fileList.cend())
            {
                // no. add comma.
                outStream << ",";
                // add break after every entry and add indent again
                outStream << std::endl
                          << indent;
            }
        }
        outStream << std::endl
                  << "};" << std::endl
                  << std::endl;
        // create map
        outStream << "res2hMapType res2hMap(mapTemp, mapTemp + sizeof mapTemp / sizeof mapTemp[0]);" << std::endl;
    }
    // close file
    outStream.close();
    IF_BEVERBOSE(std::cout << " - succeeded." << std::endl)
    return true;
}

static bool createBlob(const std::vector<FileData> &fileList, const stdfs::path &filePath)
{
    // try opening the output file. truncate it when it exists
    std::fstream outStream;
    outStream.open(filePath.string(), std::ofstream::out | std::ofstream::binary | std::ofstream::trunc);
    if (!outStream.is_open() || !outStream.good())
    {
        std::cerr << "Failed to open file \"" << filePath.string() << "\" for writing" << std::endl;
        return false;
    }
    const auto nrOfEntries = static_cast<uint32_t>(fileList.size());
    // check if a 64bit archive is needed, or 32bits suffice
    uint64_t directorySize = 0;
    uint64_t maxDataSize = 0;
    uint64_t dataSize = 0;
    for (const auto &file : fileList)
    {
        dataSize += file.size;
        maxDataSize = maxDataSize < file.size ? file.size : maxDataSize;
        directorySize += file.internalName.size();
    }
    // now take worst case header and fixed directory size into account and check if we need 32 or 64 bit
    const bool mustUse64Bit = maxDataSize > UINT32_MAX || (RES2H_HEADER_SIZE_64 + directorySize + nrOfEntries * RES2H_DIRECTORY_SIZE_64 + dataSize + sizeof(uint64_t)) > UINT32_MAX;
    IF_BEVERBOSE(std::cout << std::endl
                           << "Creating binary " << (mustUse64Bit ? "64" : "32") << "bit archive " << filePath << std::endl)
    // now that we know how many bits, add the correct amount of data for the fixed directory entries to the variable
    directorySize += nrOfEntries * (mustUse64Bit ? RES2H_DIRECTORY_SIZE_64 : RES2H_DIRECTORY_SIZE_32);
    // add magic number to file
    outStream.write(reinterpret_cast<const char *>(&RES2H_MAGIC_BYTES), sizeof(RES2H_MAGIC_BYTES) - 1);
    // add version and format flag to file
    const uint32_t fileVersion = RES2H_ARCHIVE_VERSION;
    const uint32_t fileFlags = mustUse64Bit ? 64 : 32;
    outStream.write(reinterpret_cast<const char *>(&fileVersion), sizeof(uint32_t));
    outStream.write(reinterpret_cast<const char *>(&fileFlags), sizeof(uint32_t));
    // add dummy archive size to file
    uint64_t archiveSize = 0;
    outStream.write(reinterpret_cast<const char *>(&archiveSize), (mustUse64Bit ? sizeof(uint64_t) : sizeof(uint32_t)));
    // add number of directory entries to file
    outStream.write(reinterpret_cast<const char *>(&nrOfEntries), sizeof(uint32_t));
    // calculate data start offset behind directory
    uint64_t dataStart = mustUse64Bit ? RES2H_HEADER_SIZE_64 : RES2H_HEADER_SIZE_32;
    dataStart += directorySize;
    // add directory for all files
    for (const auto &file : fileList)
    {
        // add size of name
        if (file.internalName.size() > UINT16_MAX)
        {
            std::cerr << "File name \"" << file.internalName << "\" is too long" << std::endl;
            outStream.close();
            return false;
        }
        const auto nameSize = static_cast<uint16_t>(file.internalName.size());
        outStream.write(reinterpret_cast<const char *>(&nameSize), sizeof(uint16_t));
        // add name
        outStream.write(reinterpret_cast<const char *>(&file.internalName[0]), nameSize);
        // add flags
        const uint32_t entryFlags = 0;
        outStream.write(reinterpret_cast<const char *>(&entryFlags), sizeof(uint32_t));
        uint64_t fileChecksum = 0;
        try
        {
            fileChecksum = mustUse64Bit ? calculateFletcher<uint64_t>(file.inPath.string()) : calculateFletcher<uint32_t>(file.inPath.string());
        }
        catch (const std::runtime_error &e)
        {
            std::cerr << "Failed to calculate file checksum: " << e.what() << std::endl;
            return false;
        }
        // add data size, offset from file start to start of data and checksum
        outStream.write(reinterpret_cast<const char *>(&file.size), (mustUse64Bit ? sizeof(uint64_t) : sizeof(uint32_t)));
        outStream.write(reinterpret_cast<const char *>(&dataStart), (mustUse64Bit ? sizeof(uint64_t) : sizeof(uint32_t)));
        outStream.write(reinterpret_cast<const char *>(&fileChecksum), (mustUse64Bit ? sizeof(uint64_t) : sizeof(uint32_t)));
        IF_BEVERBOSE(std::cout << "Creating directory entry for \"" << file.internalName << "\"" << std::endl)
        IF_BEVERBOSE(std::cout << "Data starts at " << std::dec << std::showbase << dataStart << " bytes" << std::endl)
        IF_BEVERBOSE(std::cout << "Size is " << std::dec << file.size << " bytes" << std::endl)
        IF_BEVERBOSE(std::cout << "Fletcher" << (mustUse64Bit ? "64" : "32") << " checksum is " << std::hex << std::showbase << fileChecksum << std::endl)
        // now add size of this entries data to start offset for next data block
        dataStart += file.size;
    }
    // add data for all files
    for (const auto &file : fileList)
    {
        // try to open file
        std::ifstream inStream;
        inStream.open(file.inPath.string(), std::ios_base::in | std::ios_base::binary);
        if (!inStream.is_open() || !inStream.good())
        {
            std::cerr << "Failed to open file \"" << file.inPath.string() << "\" for reading" << std::endl;
            outStream.close();
            return false;
        }
        IF_BEVERBOSE(std::cout << "Adding data for \"" << file.internalName << "\"" << std::endl)
        uint64_t overallDataSize = 0;
        // copy data from input to output file
        while (!inStream.eof() && inStream.good())
        {
            std::array<uint8_t, 4096> buffer{};
            std::streamsize readSize = sizeof(buffer);
            try
            {
                // try reading data from input file
                inStream.read(reinterpret_cast<char *>(buffer.data()), sizeof(buffer));
            }
            catch (const std::ios_base::failure & /*e*/)
            {
                // if something other that EOF happended, this is a serious error
                if (!inStream.eof())
                {
                    std::cerr << "While reading file \"" << file.inPath.string() << "\"" << std::endl;
                    outStream.close();
                    return false;
                }
            }
            readSize = inStream.gcount();
            // write to output file and increase size of overall data read
            outStream.write(reinterpret_cast<const char *>(buffer.data()), readSize);
            overallDataSize += static_cast<uint64_t>(readSize);
        }
        // close input file
        inStream.close();
        // check if the file was completely read
        if (overallDataSize != file.size)
        {
            std::cerr << "Failed to completely copy file \"" << file.inPath.string() << "\" to binary data" << std::endl;
            outStream.close();
            return false;
        }
    }
    // final archive size is current size + checksum. write size to the header now
    archiveSize = static_cast<uint64_t>(outStream.tellg()) + (mustUse64Bit ? sizeof(uint64_t) : sizeof(uint32_t));
    outStream.seekg(RES2H_OFFSET_ARCHIVE_SIZE);
    outStream.write(reinterpret_cast<const char *>(&archiveSize), (mustUse64Bit ? sizeof(uint64_t) : sizeof(uint32_t)));
    // close file
    outStream.close();
    IF_BEVERBOSE(std::cout << "Binary archive creation succeeded." << std::endl)
    IF_BEVERBOSE(std::cout << "Archive has " << std::dec << archiveSize << " bytes." << std::endl)
    // calculate checksum of whole file and append to file
    uint64_t checksum = 0;
    try
    {
        checksum = mustUse64Bit ? calculateFletcher<uint64_t>(filePath.string()) : calculateFletcher<uint32_t>(filePath.string());
    }
    catch (const std::runtime_error &e)
    {
        std::cerr << "Failed to calculate archive checksum: " << e.what() << std::endl;
        return false;
    }
    outStream.open(filePath.string(), std::ofstream::out | std::ofstream::binary | std::ofstream::app);
    if (!outStream.is_open() || !outStream.good())
    {
        std::cerr << "Failed to open file \"" << filePath.string() << "\" for writing" << std::endl;
        return false;
    }
    outStream.seekg(0, std::ios::end);
    outStream.write(reinterpret_cast<const char *>(&checksum), (mustUse64Bit ? sizeof(uint64_t) : sizeof(uint32_t)));
    outStream.close();
    IF_BEVERBOSE(std::cout << "Archive Fletcher" << (mustUse64Bit ? "64" : "32") << " checksum is " << std::hex << std::showbase << checksum << "." << std::endl)
    return true;
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
    if (argc < 3 || !readArguments(arguments))
    {
        printUsage();
        return 2;
    }
    // check if the input path exist
    if (!stdfs::exists(inFilePath))
    {
        std::cerr << "Invalid input file/directory " << inFilePath << std::endl;
        return 1;
    }
    if (createBinary)
    {
        // check if argument 2 is a file
        if (stdfs::is_directory(outFilePath))
        {
            std::cerr << "Output must be a file if -b is used" << std::endl;
            return 1;
        }
    }
    else if (appendFile)
    {
        // check if argument 2 is a file
        if (stdfs::is_directory(outFilePath))
        {
            std::cerr << "Output must be a file if -a is used" << std::endl;
            return 1;
        }
    }
    else if (stdfs::is_directory(inFilePath) != stdfs::is_directory(outFilePath))
    {
        // check if output directory exists
        if (stdfs::is_directory(outFilePath) && !stdfs::exists(outFilePath))
        {
            std::cerr << "Invalid output directory " << outFilePath << std::endl;
            return 1;
        }
        // check if arguments 1 and 2 are both files or both directories
        std::cerr << "Input and output file must be both either a file or a directory" << std::endl;
        return 1;
    }
    if (appendFile)
    {
        // append file a to b
        try
        {
            appendFileContent(outFilePath, inFilePath);
        }
        catch (const std::runtime_error &e)
        {
            std::cerr << "Failed to append data to executable: " << e.what() << std::endl;
            return 1;
        }
    }
    else
    {
        // build list of files to process
        std::vector<FileData> fileList;
        if (stdfs::is_directory(inFilePath))
        {
            // both files are directories, build file ist
            fileList = getFileDataFrom(inFilePath, outFilePath, inFilePath, useRecursion);
            if (fileList.empty())
            {
                std::cerr << "Found no files to convert" << std::endl;
                return 1;
            }
        }
        else
        {
            // just add single input/output file
            FileData temp;
            temp.inPath = inFilePath;
            temp.outPath = outFilePath;
            temp.internalName = inFilePath.filename().string(); // remove all, but the file name and extension
            IF_BEVERBOSE(std::cout << "Found input file " << inFilePath << std::endl)
            IF_BEVERBOSE(std::cout << "Internal name will be \"" << temp.internalName << "\"" << std::endl)
            IF_BEVERBOSE(std::cout << "Output path is " << temp.outPath << std::endl)
            // get file size
            try
            {
                temp.size = static_cast<uint64_t>(stdfs::file_size(inFilePath));
                IF_BEVERBOSE(std::cout << "Size is " << temp.size << " bytes." << std::endl)
            }
            catch (const stdfs::filesystem_error &e)
            {
                std::cerr << "Failed to get size of " << inFilePath << ": " << e.what() << std::endl;
                temp.size = 0;
            }
            fileList.push_back(temp);
        }
        // does the user want an binary file?
        if (createBinary)
        {
            // yes. build it.
            if (!createBlob(fileList, outFilePath))
            {
                std::cerr << "Failed to convert to binary file" << std::endl;
                return 1;
            }
        }
        else
        {
            // FIXME: Clang-tidy complains that the following functions can throw an exception
            // That is not caught before the end of main(). Make sure they don't.
            try
            {
                // no. convert files to .c/.cpp. loop through list, converting files
                for (auto fdIt = fileList.begin(); fdIt != fileList.cend(); ++fdIt)
                {
                    if (!convertFile(*fdIt, commonHeaderFilePath))
                    {
                        std::cerr << "Failed to convert all files. Aborting" << std::endl;
                        return 1;
                    }
                }
                // do we need to write a header file?
                if (!commonHeaderFilePath.empty())
                {
                    if (!createCommonHeader(fileList, commonHeaderFilePath, !utilitiesFilePath.empty(), useC))
                    {
                        std::cerr << "Failed to create common header file" << std::endl;
                        return 1;
                    }
                    // do we need to create utilities?
                    if (!utilitiesFilePath.empty())
                    {
                        if (!createUtilities(fileList, utilitiesFilePath, commonHeaderFilePath, useC, combineResults))
                        {
                            std::cerr << "Failed to create utilities file" << std::endl;
                            return 1;
                        }
                    }
                }
            }
            catch (...)
            {
            }
        }
    }
    // profit!!!
    std::cout << "res2h succeeded." << std::endl;
    return 0;
}
