#pragma once

#include <stdint.h>
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
		std::string fileName; //!<Name of file. If it starts with ":/" it is considered an internal file in a binary archive.
		std::shared_ptr<unsigned char *> data; //!<File content.
		size_t size; //!<Content size.
		size_t dataOffset; //!<Offset in binary archive if any.
        uint32_t checksum; //!<Checksum of raw stored data.
        std::string archivePath; //!<Path on disk to binary archive file.
		size_t archiveOffset; //!<Offset of the archive data in the archive file. Used when an archive is embedded e.g. in an executable.
	};

private:
	static std::map<std::string, ResourceEntry> resourceMap;

public:
	/*!
	Open binary archive from disk and load directory into memory. You can add as many archives as you want.
	\param[in] archivePath Archive path.
	\param[in] embeddedArchive Optional. Pass true if the archive data is appended to another file. It will be searched from the end of the file towards the start.
	\note Archives can be appended to other files using res2h: "res2h <ARCHIVE> <APPEND_TARGET> -a"
	\return Returns true if opening and loading the archive directory worked.
	*/
	static bool loadArchive(const std::string & archivePath, bool embeddedArchive = false);

	/*!
	Load file content. Can be either a file on disk or a file in a binary archive.
	\param[in] filePath Path to the file. If it start with ":/" it is considered to be in a binary archive.
	\param[in] keepInCache Pass true to keep the resource in memory if you need it more than once.
	\return Returns a struct containing the data.
	\note When loading data from a binary archive, load it with \loadArchive before.
	*/
	static ResourceEntry loadFile(const std::string & filePath, bool keepInCache = false);

	/*!
	Release all cached data. Keeps directories in memory.
    \note This releases the shared_ptr to the data. If you keep more instances of that shared_ptr, memory will NOT be freed!
	*/
	static void releaseCache();

	/*
	Create Adler-32 checksum from file. Builds checksum from start position till EOF.
	\param[in] filePath Path to the file to build the checksum for.
	\param[in] adler Optional. Adler checksum from last run if you're using more than one file.
	\return Returns the Adler-32 checksum for the file stream or the initial checksum upon failure.
	\note Based on the sample code here: https://tools.ietf.org/html/rfc1950. This is not as safe as CRC-32 (see here: https://en.wikipedia.org/wiki/Adler-32), but should be totally sufficient for us.
	*/
	static uint32_t calculateAdler32(const std::string & filePath, uint32_t adler = 1);
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
