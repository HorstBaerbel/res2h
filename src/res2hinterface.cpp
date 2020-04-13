#include "res2hinterface.h"

#include <climits>
#include <fstream>
#include <utility>

Res2hException::Res2hException(const char *errorString) noexcept
    : std::runtime_error(errorString)
{
}

Res2h &Res2h::instance()
{
    static Res2h instance;
    return instance;
}

uint64_t Res2h::findArchiveStartOffset(const std::string &archivePath) const
{
    // check if the archive is in our list already
    for (auto &entry : m_archives)
    {
        if (entry.archive.filePath == archivePath)
        {
            return entry.archive.offsetInFile;
        }
    }
    // no. open archive
    std::ifstream inStream;
    inStream.open(archivePath, std::ios_base::in | std::ios_base::binary);
    if (!inStream.is_open() || !inStream.good())
    {
        throw Res2hException("Failed to open archive for reading");
    }
    // try to read magic bytes
    std::array<char, 8> magicBytes{};
    inStream.read(reinterpret_cast<char *>(magicBytes.data()), sizeof(magicBytes));
    std::string magicString(reinterpret_cast<char *>(magicBytes.data()), sizeof(magicBytes));
    if (magicString == RES2H_MAGIC_BYTES)
    {
        // found. close and return offset
        inStream.close();
        return 0;
    }
    // no magic bytes at start. might be an embedded archive, search for a header backwards from EOF...
    std::array<char, 4096> buffer{};
    // seek to end minus buffer size
    inStream.seekg(sizeof(buffer), std::ios::end);
    // read whole file to search for archive
    while (inStream.good())
    {
        // read block of data and convert to string
        inStream.read(reinterpret_cast<char *>(buffer.data()), sizeof(buffer));
        std::string haystack(reinterpret_cast<char *>(buffer.data()), sizeof(buffer));
        // try to find magic bytes
        auto magicPosition = haystack.rfind(RES2H_MAGIC_BYTES);
        if (magicPosition != std::string::npos)
        {
            // found. close and return offset
            inStream.close();
            return static_cast<uint64_t>(inStream.tellg()) + magicPosition;
        }
        // check if we're already at the start of the stream
        if (inStream.tellg() == std::streampos(0))
        {
            break;
        }
        // no. seek the size of the buffer in direction of the start of the file,
        // but read some bytes again, else we could miss the header in between buffer
        inStream.seekg(-static_cast<int64_t>(sizeof(buffer) - sizeof(RES2H_MAGIC_BYTES) + 1), std::ios::cur);
    }
    // close file. nothing found.
    inStream.close();
    throw Res2hException("No valid archive found");
}

Res2h::ArchiveInfo Res2h::archiveInfo(const std::string &archivePath) const
{
    // check if the archive is in our list already
    for (auto &entry : m_archives)
    {
        if (entry.archive.filePath == archivePath)
        {
            return entry.archive;
        }
    }
    // no. try to find archive start in file. If it is not found it will throw. we don't catch the exception here, but simply pass it on...
    ArchiveInfo info;
    info.filePath = archivePath;
    info.offsetInFile = findArchiveStartOffset(archivePath);
    const std::streamsize nrOfBytesSizeOrChecksum = info.bits == 64 ? sizeof(uint64_t) : sizeof(uint32_t);
    // open archive again
    std::ifstream inStream;
    inStream.open(archivePath, std::ios_base::in | std::ios_base::binary);
    if (!inStream.is_open() || !inStream.good())
    {
        throw Res2hException("Failed to open archive for reading");
    }
    // nice. magic bytes ok. read file version
    inStream.seekg(static_cast<std::streamoff>(info.offsetInFile + RES2H_OFFSET_FILE_VERSION));
    inStream.read(reinterpret_cast<char *>(&info.fileVersion), sizeof(uint32_t));
    if (info.fileVersion != RES2H_ARCHIVE_VERSION)
    {
        inStream.close();
        throw Res2hException("Bad archive file version");
    }
    // check file flags (32/64 bit)
    inStream.seekg(static_cast<std::streamoff>(info.offsetInFile + RES2H_OFFSET_FORMAT_FLAGS));
    inStream.read(reinterpret_cast<char *>(&info.formatFlags), sizeof(uint32_t));
    // the low 8 bit of the flags is the archive bit depth, e.g. 32/64 bit
    info.bits = info.formatFlags & 0x000000FF;
    if (info.bits != 32 && info.bits != 64)
    {
        inStream.close();
        throw Res2hException("Unsupported archive bit depth");
    }
    // get size of the whole archive.
    uint64_t archiveSize = 0;
    inStream.seekg(static_cast<std::streamoff>(info.offsetInFile + RES2H_OFFSET_ARCHIVE_SIZE));
    inStream.read(reinterpret_cast<char *>(&archiveSize), nrOfBytesSizeOrChecksum);
    info.size = archiveSize;
    if (info.size <= 0)
    {
        inStream.close();
        throw Res2hException("Archive has an internal size of 0");
    }
    // read checksum from end of file
    uint64_t readChecksum = 0;
    inStream.seekg(static_cast<std::streamoff>(info.offsetInFile + info.size - static_cast<uint64_t>(nrOfBytesSizeOrChecksum)));
    inStream.read(reinterpret_cast<char *>(&readChecksum), nrOfBytesSizeOrChecksum);
    info.checksum = readChecksum;
    // control checksum. close first for calling checksum function
    inStream.close();
    uint64_t fileChecksum = (info.bits == 64 ? calculateFletcher<uint64_t>(archivePath, info.size - sizeof(uint64_t)) : calculateFletcher<uint32_t>(archivePath, static_cast<uint32_t>(info.size - sizeof(uint32_t))));
    if (info.checksum != fileChecksum)
    {
        throw Res2hException("Archive has a bad checksum");
    }
    inStream.close();
    return info;
}

void Res2h::releaseData()
{
    // erase all allocated data in resource map
    for (auto &entry : m_archives)
    {
        for (auto &resource : entry.resources)
        {
            resource.data.clear();
        }
    }
}

bool Res2h::loadArchive(const std::string &archivePath)
{
    // check if there are entries for this archive already in the map and delete them
    auto aIt = m_archives.begin();
    while (aIt != m_archives.end())
    {
        if (aIt->archive.filePath == archivePath)
        {
            // same archive. erase and reload info
            aIt = m_archives.erase(aIt);
        }
        else
        {
            ++aIt;
        }
    }
    // try to find archive in file. this will throw if it fails
    ArchiveInfo info = archiveInfo(archivePath);
    // open archive
    std::ifstream inStream;
    inStream.open(archivePath, std::ios_base::in | std::ios_base::binary);
    if (!inStream.is_open() || !inStream.good())
    {
        throw Res2hException("Failed to open archive for reading");
    }
    ArchiveEntry entry;
    entry.archive = info;
    // file version ok. skip currently unused flags and archive size
    inStream.seekg(static_cast<std::streamoff>(info.offsetInFile + (info.bits == 64 ? RES2H_OFFSET_NO_OF_FILES_64 : RES2H_OFFSET_NO_OF_FILES_32)));
    // read number of directory entries
    uint32_t nrOfDirectoryEntries = 0;
    inStream.read(reinterpret_cast<char *>(&nrOfDirectoryEntries), sizeof(uint32_t));
    for (uint32_t i = 0; i < nrOfDirectoryEntries; ++i)
    {
        // read directory
        ResourceInfo temp;
        // read size of name
        uint16_t sizeOfName = 0;
        inStream.read(reinterpret_cast<char *>(&sizeOfName), sizeof(uint16_t));
        // read name itself
        temp.filePath.resize(sizeOfName);
        inStream.read(reinterpret_cast<char *>(&temp.filePath[0]), sizeOfName);
        // skip currently unused flags
        inStream.seekg(sizeof(uint32_t), std::ios::cur);
        // read size of data
        inStream.read(reinterpret_cast<char *>(&temp.dataSize), (info.bits == 64 ? sizeof(uint64_t) : sizeof(uint32_t)));
        // read offset to start of data
        inStream.read(reinterpret_cast<char *>(&temp.dataOffset), (info.bits == 64 ? sizeof(uint64_t) : sizeof(uint32_t)));
        // read data checksum
        inStream.read(reinterpret_cast<char *>(&temp.checksum), (info.bits == 64 ? sizeof(uint64_t) : sizeof(uint32_t)));
        // add to resources
        entry.resources.push_back(temp);
    }
    // close file and add entry
    inStream.close();
    m_archives.push_back(entry);
    return true;
}

Res2h::ResourceInfo Res2h::loadResource(const std::string &filePath, bool keepInCache, bool checkChecksum)
{
    ResourceInfo temp;
    // check if from archive or disk
    if (filePath.find_first_of(":/") == 0)
    {
        // find file in the archive
        for (auto &entry : m_archives)
        {
            for (auto &resource : entry.resources)
            {
                if (resource.filePath == filePath)
                {
                    // file found. check if data is in memory
                    if (!resource.data.empty())
                    {
                        return resource;
                    }
                    // no. load data first
                    auto tempEntry = loadResourceFromArchive(resource, entry.archive, checkChecksum);
                    if (keepInCache)
                    {
                        resource = tempEntry;
                    }
                    return tempEntry;
                }
            }
        }
        throw Res2hException("Failed to load file from archive");
    }
    // find file in the disk resources list
    for (auto &resource : m_diskResources)
    {
        if (resource.filePath == filePath)
        {
            // file found. check if data is in memory
            if (!resource.data.empty())
            {
                return resource;
            }
            // no. load data first
            auto tempEntry = loadResourceFromDisk(filePath);
            if (keepInCache)
            {
                resource = tempEntry;
            }
            return tempEntry;
        }
    }
    throw Res2hException("Failed to load file from disk");
}

Res2h::ResourceInfo Res2h::loadResourceFromDisk(const std::string &filePath)
{
    ResourceInfo temp;
    // try to open file
    std::ifstream inStream;
    inStream.open(filePath, std::ios_base::in | std::ios_base::binary);
    if (!inStream.is_open() || !inStream.good())
    {
        throw Res2hException("Failed to open file for reading");
    }
    // opened ok. check file size
    inStream.seekg(0, std::ios::end);
    auto fileSize = inStream.tellg();
    inStream.seekg(0);
    if (fileSize > 0)
    {
        // allocate and try reading data
        std::vector<uint8_t> fileData(static_cast<std::size_t>(fileSize));
        try
        {
            inStream.read(reinterpret_cast<char *>(fileData.data()), fileSize);
        }
        catch (const std::ios_base::failure & /*e*/)
        {
            // ignore exception and check how many bytes were actually read
        }
        if (inStream.gcount() != fileSize)
        {
            throw Res2hException("Failed to read file from disk");
        }
        // seems to have worked. store data.
        temp.filePath = filePath;
        temp.data = fileData;
        temp.dataSize = static_cast<uint64_t>(fileSize);
    }
    return temp;
}

Res2h::ResourceInfo Res2h::loadResourceFromArchive(const ResourceInfo &entry, const ArchiveInfo &archive, bool checkChecksum)
{
    ResourceInfo temp = entry;
    // try to open archive file
    std::ifstream inStream;
    inStream.open(archive.filePath, std::ios_base::in | std::ios_base::binary);
    if (!inStream.is_open() || !inStream.good())
    {
        throw Res2hException("Failed to open file for reading");
    }
    // opened ok. move to data offset
    inStream.seekg(static_cast<std::streamoff>(archive.offsetInFile + temp.dataOffset));
    // allocate and try reading data
    temp.data = std::vector<uint8_t>(temp.dataSize);
    try
    {
        inStream.read(reinterpret_cast<char *>(temp.data.data()), static_cast<std::streamsize>(temp.dataSize));
    }
    catch (const std::ios_base::failure & /*e*/)
    {
        // ignore exception and check how many bytes were actually read
    }
    if (static_cast<uint64_t>(inStream.gcount()) != temp.dataSize)
    {
        throw Res2hException("Failed to read file from archive");
    }
    // now that we're here, do a checksum
    if (checkChecksum)
    {
        if (archive.bits == 32 && static_cast<uint32_t>(temp.checksum) == calculateFletcher<uint32_t>(temp.data.data(), static_cast<uint32_t>(temp.dataSize)))
        {
            return temp;
        }
        if (archive.bits == 64 && temp.checksum == calculateFletcher<uint64_t>(temp.data.data(), temp.dataSize))
        {
            return temp;
        }
        throw Res2hException("Bad file checksum");
    }
    return temp;
}

std::vector<std::reference_wrapper<const Res2h::ResourceInfo>> Res2h::resourceInfo() const
{
    std::vector<std::reference_wrapper<const ResourceInfo>> result;
    for (auto &entry : m_archives)
    {
        for (auto &resource : entry.resources)
        {
            result.emplace_back(std::reference_wrapper<const ResourceInfo>(resource));
        }
    }
    for (auto &resource : m_diskResources)
    {
        result.emplace_back(std::reference_wrapper<const ResourceInfo>(resource));
    }
    return result;
}
