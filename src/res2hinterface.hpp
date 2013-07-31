#pragma once

#include <cstdint>
#include <string>
#include <memory>
#include <map>
#include <exception>

#include "res2h.h"


class Res2h
{
public:
	struct ResourceEntry
	{
		std::string filePath; //!<Name of file. If it starts with ":/" it is considered an internal file in a binary res2h archive.
		std::shared_ptr<unsigned char> data; //!<Raw file content.
		uint32_t dataSize; //!<Raw content size.
		uint32_t dataOffset; //!<Raw content offset in binary res2h archive if any.
        uint32_t checksum; //!<Checksum of raw content.
        std::string archivePath; //!<Path on disk to binary res2h archive or to the file the archive is embeded in.
		uint32_t archiveStart; //!<Offset of the archive data in the file (> 0 when an archive is embedded e.g. in an executable).

        ResourceEntry() : dataSize(0), dataOffset(0), checksum(0), archiveStart(0) {}
	};

private:
	static std::map<std::string, ResourceEntry> resourceMap;

    static ResourceEntry loadFileFromDisk(const std::string & filePath);
    static ResourceEntry loadFileFromArchive(const Res2h::ResourceEntry & entry);

public:
	/*
	Check if a file is actually an archive or contains an embedded archive.
	\param[in] archivePath Archive path.
	\return Returns true if archive can be openend and its magic bytes header is found.
	*/
	static bool isArchive(const std::string & archivePath);

	/*!
	Open archive file or file with embedded archive from disk and load directory into memory. You can add as many archives as you want.
	\param[in] archivePath Archive path.
	\note Archives can be appended to each other files using res2h: "res2h <ARCHIVE> <APPEND_TARGET> -a"
	\return Returns true if opening and loading the archive directory worked.
	*/
	static bool loadArchive(const std::string & archivePath);

	/*!
	Load file content. Can be either a file on disk or a file in a binary archive.
	\param[in] filePath Path to the file. If it start with ":/" it is considered to be in a binary archive.
	\param[in] keepInCache Optional. Pass true to keep the resource in memory if you need it more than once. Memory archive data is never cached, because it is already in memory.
	\param[in] checkChecksum Optional. Pass true to check the calculated checksum of the data against the checksum stored in the archive.
	\return Returns a struct containing the data.
	\note When loading data from a binary archive, load it with \loadArchive before.
	*/
	static ResourceEntry loadFile(const std::string & filePath, bool keepInCache = false, bool checkChecksum = true);

	/*!
	Return the number of resources currently in the map.
	\return Returns the number of resources currently loaded.
	*/
	static size_t getNrOfResources();

	/*!
	Return a resource from the map.
	\param[in] index The index of the resource to return.
	\return Returns the index'th resource from the map.
	*/
	static ResourceEntry getResource(size_t index);

	/*!
	Release all cached data. Keeps directories in memory.
    \note This releases the shared_ptr to the data. If you keep more instances of that shared_ptr, memory will NOT be freed!
	*/
	static void releaseCache();

	/*
	Create Adler-32 checksum from file. Builds checksum from start position till EOF.
	\param[in] filePath Path to the file to build the checksum for.
	\param[in] dataSize Optional. The size of the data to incorporate in the checksum. Pass 0 to scan whole file.
	\param[in] adler Optional. Adler checksum from last run if you're using more than one file.
	\return Returns the Adler-32 checksum for the file stream or the initial checksum upon failure.
	\note Based on the sample code here: https://tools.ietf.org/html/rfc1950. This is not as safe as CRC-32 (see here: https://en.wikipedia.org/wiki/Adler-32), but should be totally sufficient for us.
	*/
	static uint32_t calculateChecksum(const std::string & filePath, const size_t dataSize = 0, uint32_t adler = 1);
};

//-----------------------------------------------------------------------------

class Res2hException : public std::exception
{
	std::string error;

	Res2hException();

public:
    Res2hException(const char * errorString) throw();
	Res2hException(const std::string & errorString) throw();

	virtual const char* what() const throw();
	virtual const std::string & whatString() const throw();
};
