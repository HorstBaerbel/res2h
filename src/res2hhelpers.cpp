#include "res2hhelpers.h"

#include "stdfshelpers.h"

#include <algorithm>
#include <iostream>

std::vector<FileData> getFileData(const stdfs::path &inPath, const stdfs::path &parentDir, bool recurse, bool beVerbose)
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
            if (beVerbose)
            {
                std::cout << "Found input file " << filePath << std::endl;
            }
            FileData temp;
            temp.inPath = filePath;
            auto subPath = naiveRelative(filePath, parentDir);
            // add a ":/" before the name to mark internal resources (Yes. Hello Qt!)
            temp.internalName = ":/" + subPath.generic_string();
            // remove parent directory of file from path for internal name
            if (beVerbose)
            {
                std::cout << "File path: " << filePath << std::endl;
                std::cout << "Parent dir: " << parentDir << std::endl;
                std::cout << "Internal name will be \"" << temp.internalName << "\"" << std::endl;
            }
            try
            {
                // get file size and add to file list
                temp.size = static_cast<uint64_t>(stdfs::file_size(filePath));
                if (beVerbose)
                {
                    std::cout << "Size is " << temp.size << " bytes." << std::endl;
                }
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
                if (beVerbose)
                {
                    std::cout << "Found subdirectory " << dirPath << std::endl;
                }
                // subdirectory found. recurse and add returned result to file list
                std::vector<FileData> subFiles = getFileData(dirPath, parentDir, recurse, beVerbose);
                files.insert(files.end(), subFiles.cbegin(), subFiles.cend());
            }
        }
    }
    // return result
    return files;
}

std::vector<FileData> generateOutputPaths(const std::vector<FileData> &files, const stdfs::path &parentDir, const stdfs::path &outPath, bool useC, bool beVerbose)
{
    std::vector<FileData> result = files;
    for (auto &file : result)
    {
        if (beVerbose)
        {
            std::cout << "File path: " << file.inPath << std::endl;
        }
        // replace dots in file name with '_' and add a .c/.cpp extension
        std::string newFileName = file.inPath.filename().generic_string();
        std::replace(newFileName.begin(), newFileName.end(), '.', '_');
        if (useC)
        {
            newFileName.append(".c");
        }
        else
        {
            newFileName.append(".cpp");
        }
        auto subPath = naiveRelative(file.inPath, parentDir);
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
        file.outPath = outPath / newFileName;
        if (beVerbose)
        {
            std::cout << "Output path: " << file.outPath << std::endl;
        }
    }
    return result;
}
