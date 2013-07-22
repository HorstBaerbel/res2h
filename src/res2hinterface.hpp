#pragma once

#include <string>
#include <memory>
#include <map>
#include <exception>

#include "res2h.h"
#include "res2hutils.h"


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
	};

private:
	static std::map<std::string, ResourceEntry> resourceMap;

public:
	/*!
	Open binary archive from disk and load directory into memory. You can add as many archives as you want.
	\param[in] archivePath Archive path.
	\return Returns true if opening and loading the archive directory worked.
	*/
	static bool loadArchive(const std::string & archivePath);

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
