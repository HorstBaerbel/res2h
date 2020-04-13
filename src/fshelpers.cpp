#include "fshelpers.h"

#include <fstream>

// This is based on the example code found here: https://svn.boost.org/trac/boost/ticket/1976
stdfs::path naiveRelative(const stdfs::path &path, const stdfs::path &base)
{
    if (path.has_root_path())
    {
        // make sure the roots of both paths match, otherwise they're on different drives
        if (path.root_path() != base.root_path())
        {
            return path;
        }
        return naiveRelative(path.relative_path(), base.relative_path());
    }
    if (base.has_root_path())
    {
        return path;
    }
    // skip all directories both paths have in common
    auto pathIt = path.begin();
    auto baseIt = base.begin();
    while (pathIt != path.end() && baseIt != base.end())
    {
        if (*pathIt != *baseIt)
        {
            break;
        }
        ++pathIt;
        ++baseIt;
    }
    stdfs::path result;
    // now if there's directories left over in base, we need to add .. for every level
    while (baseIt != base.end())
    {
        if (*baseIt != "." && *baseIt != base.filename())
        {
            result /= "..";
        }
        ++baseIt;
    }
    // now if there's directories left over in path add them to the result
    while (pathIt != path.end())
    {
        if (*pathIt != ".")
        {
            result /= *pathIt;
        }
        ++pathIt;
    }
    return result != "." ? result : stdfs::path();
}

stdfs::path naiveLexicallyNormal(const stdfs::path &path, const stdfs::path &base)
{
    stdfs::path result;
    stdfs::path absPath = stdfs::absolute(path, base);
    for (const auto & part : absPath)
    {
        if (part == "..")
        {
            // /a/b/.. is not necessarily /a if b is a symbolic link
            // /a/b/../.. is not /a/b/.. under most circumstances
            // We can end up with ..s in our result because of symbolic links
            if (stdfs::is_symlink(result) || result.filename() == "..")
            {
                result /= part;
            }
            // otherwise it should be safe to resolve the parent
            else
            {
                result = result.parent_path();
            }
        }
        else if (part == ".")
        {
            // ignore all .
        }
        else
        {
            // append other path entries
            result /= part;
        }
    }
    return result;
}

bool startsWithPrefix(const stdfs::path &path, const stdfs::path &prefix)
{
    if (path.empty() || prefix.empty())
    {
        return false;
    }
    auto pair = std::mismatch(path.begin(), path.end(), prefix.begin(), prefix.end());
    return pair.second == prefix.end();
}

bool hasRecursiveSymlink(const stdfs::path &path)
{
    if (stdfs::is_symlink(path))
    {
        // check if the symlink points somewhere in the path. this would recurse
        if (startsWithPrefix(stdfs::canonical(path), path))
        {
            return true;
        }
    }
    return false;
}

void appendFileContent(const stdfs::path &dstFile, const stdfs::path &srcFile)
{
    // try opening the output file.
    std::fstream outStream;
    outStream.open(dstFile.string(), std::ofstream::out | std::ofstream::binary | std::ofstream::app);
    if (!outStream.is_open() || !outStream.good())
    {
        throw std::runtime_error("Failed to open destination file for writing");
    }
    // seek to the end
    outStream.seekg(0, std::ios::end);
    // open input file
    std::ifstream inStream;
    inStream.open(srcFile.string(), std::ios_base::in | std::ios_base::binary);
    if (!inStream.is_open() || !inStream.good())
    {
        throw std::runtime_error("Failed to open source file for reading");
    }
    // copy data from input to output file
    while (!inStream.eof() && inStream.good())
    {
        std::array<uint8_t, 4096> buffer{};
        std::streamsize readSize = sizeof(buffer);
        // try reading data from input file
        try
        {
            inStream.read(reinterpret_cast<char *>(buffer.data()), sizeof(buffer));
        }
        catch (const std::ios_base::failure & /*e*/)
        {
            // if something other than EOF happended, this is a serious error
            if (!inStream.eof())
            {
                throw std::runtime_error("Failed to read from source file");
            }
        }
        // store how many bytes were actually read
        readSize = inStream.gcount();
        // try writing to output file
        try
        {
            outStream.write(reinterpret_cast<const char *>(buffer.data()), readSize);
        }
        catch (const std::ios_base::failure & /*e*/)
        {
            throw std::runtime_error("Failed to write to destination file");
        }
    }
}

bool compareFileContent(const stdfs::path &a, const stdfs::path &b)
{
    // try opening first file.
    std::ifstream aStream;
    aStream.open(a.string(), std::ofstream::binary);
    if (!aStream.is_open() || !aStream.good())
    {
        throw std::runtime_error("Failed to open file a for reading");
    }
    // try opening second file
    std::ifstream bStream;
    bStream.open(b.string(), std::ifstream::binary);
    if (!bStream.is_open() || !bStream.good())
    {
        throw std::runtime_error("Failed to open file b for reading");
    }
    while (!aStream.eof() && aStream.good() && !bStream.eof() && bStream.good())
    {
        std::array<uint8_t, 4096> bufferA{};
        std::array<uint8_t, 4096> bufferB{};
        std::streamsize readSizeA = sizeof(bufferA);
        std::streamsize readSizeB = sizeof(bufferB);
        // try reading first file
        try
        {
            aStream.read(reinterpret_cast<char *>(bufferA.data()), sizeof(bufferA));
        }
        catch (const std::ios_base::failure & /*e*/)
        {
            // if something other than EOF happended, this is a serious error
            if (!aStream.eof())
            {
                throw std::runtime_error("Failed to read from file a");
            }
        }
        // try reading second file
        try
        {
            bStream.read(reinterpret_cast<char *>(bufferB.data()), sizeof(bufferB));
        }
        catch (const std::ios_base::failure & /*e*/)
        {
            // if something other than EOF happended, this is a serious error
            if (!bStream.eof())
            {
                throw std::runtime_error("Failed to read from file b");
            }
        }
        // store how many bytes were actually read
        readSizeA = aStream.gcount();
        readSizeB = bStream.gcount();
        // check if size is the same
        if (readSizeA == readSizeB)
        {
            // check if data block is the same
            if (!std::equal(bufferA.cbegin(), bufferA.cend(), bufferB.cbegin()))
            {
                return false;
            }
        }
        else
        {
            return false;
        }
    }
    return true;
}
