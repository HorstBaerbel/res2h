#pragma once

#include "stdfs.h"

#include <cstdint>
#include <string>
#include <vector>

/// @brief Information about source files for archive or conversion.
struct FileData
{
    stdfs::path inPath;
    stdfs::path outPath;
    std::string internalName;
    std::string dataVariableName;
    std::string sizeVariableName;
    uint64_t size = 0;
};

/// @brief Fill the FileData structure with information about files on disk.
/// @param inPath File or directory to get information for.
/// @param parentDir Parent directory for files.
/// @param recurse Recurse through subdirectories of inPath.
/// @param beVerbose Output diagnoctic information to stdout.
/// @return Returns information about files on disk in inPath.
std::vector<FileData> getFileData(const stdfs::path &inPath, const stdfs::path &parentDir, bool recurse, bool beVerbose = false);

/// @brief Fill the FileData structure with information about file output paths.
/// @param files Input files to add information to.
/// @param parentDir Parent directory for files.
/// @param outPath Path output files are being written to.
/// @param useC Generate output file names with ".c" extension instead of ".cpp".
/// @param beVerbose Output diagnoctic information to stdout.
/// @return Return updated files with output information.
std::vector<FileData> generateOutputPaths(const std::vector<FileData> &files, const stdfs::path &parentDir, const stdfs::path &outPath, bool useC, bool beVerbose = false);
