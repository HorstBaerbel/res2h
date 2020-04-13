#pragma once

#include <cstdint>
#include <exception>
#include <functional>
#include <string>
#include <vector>

#include "checksum.h"
#include "res2h.h"

/// @brief Exceptions thrown by Res2h when something goes wrong.
class Res2hException final : public std::runtime_error
{
  public:
    explicit Res2hException(const char *errorString) noexcept;

  private:
    Res2hException();
};

// -----------------------------------------------------------------------------

/// @brief Main interface class to read files from disk or from 32/64Bit res2h archives.
/// Use @sa loadFile() to load a file from disk. If the file is in a res2h archive,
/// use @sa loadArchive() before. Resources will be cached in memory until you call releaseCache(),
/// which will release all data. You can have multiple archives loaded at the same time.
/// Example:
/// Res2h::instance().loadArchive("myarchive.bin");
/// auto fileRessource = Res2h::instance().loadFile(":/myfile.txt");
/// You can also load arbitrary files from disk using:
/// auto otherRessource = Res2h::instance().loadFile("other.txt");
class Res2h
{
  public:
    struct ResourceInfo
    {
        std::string filePath; // !<Name of file. If it starts with ":/" it is considered an internal file in a binary res2h archive.
        std::vector<uint8_t> data; // !<Cached raw file content.
        uint64_t dataSize = 0; // !<Raw content size.
        uint64_t dataOffset = 0; // !<Raw content offset in binary res2h archive if any (Start of data = archive.offsetInFile + entry.dataOffset).
        uint64_t checksum = 0; // !<Fletcher-32/64 checksum of raw content.
    };

    struct ArchiveInfo
    {
        std::string filePath; // !<Path on disk to binary res2h archive or to the file the archive is embedded in.
        uint64_t offsetInFile = 0; // !<Offset of the start of the archive in the file (> 0 when an archive is embedded e.g. in an executable).
        uint32_t fileVersion = 0; // !<File format version (currently 2).
        uint32_t formatFlags = 0; // !<File option flags.
        uint8_t bits = 0; // !<Archive bit depth (32/64).
        uint64_t size = 0; // !<Overall size of archive data.
        uint64_t checksum = 0; // !<Fletcher-32/64 archive checksum.
    };

    /// @brief Return an instance of the singleton Res2h object.
    /// @return The Res2h object.
    static Res2h &instance();

    /// @brief Try to find archive header in an archive file or an embedded archive.
    /// @param archivePath Archive path.
    /// @return Returns offset if archive can be opened and its magic bytes header is found. Throws an exception otherwise.
    uint64_t findArchiveStartOffset(const std::string &archivePath) const;

    /// @brief Try to read archive header from an archive file or an embedded archive.
    /// @param archivePath Archive path.
    /// @return Returns archive info if archive can be opened and information read properly.
    /// @throw Throws a Res2hException file can't be opened or archive is corrupted
    ArchiveInfo archiveInfo(const std::string &archivePath) const;

    /// @brief Open archive file or file with embedded archive from disk and load directory into memory.
    /// You can add as many archives as you want. This does NOT load the actual data yet, only the directory.
    /// @param archivePath Archive path.
    /// @note If the archive is already loaded, all data will be released and it will be loaded all over again!
    /// @return Returns true if opening and loading the archive directory worked.
    bool loadArchive(const std::string &archivePath);

    /// @brief Load file content. Can be either a file on disk or a file in a binary archive.
    /// @param filePath Path to the file. If it start with ":/" it is considered to be in a binary archive.
    /// @param keepInCache Optional. Pass true to keep the resource in memory if you need it more than once. Memory archive data is never cached, because it is already in memory.
    /// @param checkChecksum Optional. Pass true to check the calculated checksum of the data against the checksum stored in the archive.
    /// @return Returns a struct containing the data or throws an exception if it fails to do so.
    /// @note When loading data from a binary archive, you must load the archive with @sa loadArchive() before.
    ResourceInfo loadResource(const std::string &filePath, bool keepInCache = false, bool checkChecksum = true);

    /// @brief Return information about all resources on disk and in archive, loaded or not.
    /// @return Returns information about all resources on disk and in archive, loaded or not.
    /// @note This returns const references to the resources, so no raw data is not copied.
    /// To get a reference to the resource, use get() as in resourceInfo().at(i).get().
    std::vector<std::reference_wrapper<const ResourceInfo>> resourceInfo() const;

    /// @brief Release all cached data. Keeps directories in memory.
    /// @note This releases the shared_ptr to the data. If you keep more instances of that shared_ptr, memory will NOT be freed!
    void releaseData();

  private:
    /// @brief Not default-constructible. Use instance().
    Res2h() = default;

    /// @brief Load a resource from disk.
    static ResourceInfo loadResourceFromDisk(const std::string &filePath);
    /// @brief Load a resource from a binary archive.
    static ResourceInfo loadResourceFromArchive(const ResourceInfo &entry, const ArchiveInfo &archive, bool checkChecksum);

    /// @brief Holds archive information and resources.
    struct ArchiveEntry
    {
        ArchiveInfo archive;
        std::vector<ResourceInfo> resources;
    };

    /// @brief Cache holding the archive entries.
    std::vector<ArchiveEntry> m_archives;
    /// @brief Cache holding the on-disk resources.
    std::vector<ResourceInfo> m_diskResources;
};
