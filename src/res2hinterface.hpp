#pragma once

#include <string>
#include <memory>
#include <map>


class Res2h
{
public:
	struct ResourceEntry
	{
		std::string fileName; //!<Name of file. If it starts with ":/" it is considered an internal file in a binary archive.
		std::shared_ptr<unsigned char *> data; //!<File content.
		size_t size; //!<Content size.
		std::string archivePath; //!<Path on disk to binary archive file.
		size_t archiveOffset; //!<Offset in binary archive if any.
	};

private:
	std::map<std::string, ResourceEntry> resourceMap;

	static std::weak_ptr<Res2h> res2hInstance;
	static std::shared_ptr<Res2h> & getInstance();

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
	Release all cached data. Keeps directories in memory
	*/
	static void releaseCache();
};
