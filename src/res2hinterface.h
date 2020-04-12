#pragma once

#include <cstdint>
#include <exception>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "checksum.h"
#include "res2h.h"

/*
Exceptions thrown by Res2h when something goes wrong.
*/
class Res2hException : public std::exception
{
	std::string error;

	Res2hException();

public:
	explicit Res2hException(const char * errorString) noexcept;
	explicit Res2hException(std::string  errorString) noexcept;

	const char* what() const noexcept override;
	virtual const std::string & whatString() const noexcept;
};

// -----------------------------------------------------------------------------

/*!
This is the templated main interface class to read files from disk or from res2h archives.
The template parameter defines the maximum size the archive and files can have.
Loading a file or a binary archive bigger that 4GB with Res2h<uint32_t> will throw an exception.
Use \loadFile to load a file from disk. If the file is in an res2h archive, use \loadArchive before.
Example:
Res2h<uint32_t>::getInstance().loadArchive("myarchive.bin");
auto fileRessource = Res2h<uint32_t>::getInstance().loadFile(":/myfile.txt");
*/
template <typename T>
class Res2h
{
public:
	struct ResourceInfo
	{
		std::string filePath; // !<Name of file. If it starts with ":/" it is considered an internal file in a binary res2h archive.
		std::shared_ptr<uint8_t> data; // !<Raw file content.
		T dataSize = 0; // !<Raw content size.
		T dataOffset = 0; // !<Raw content offset in binary res2h archive if any (Start of data = archive.offsetInFile + entry.dataOffset).
		T checksum = 0; // !<Fletcher-32/64 checksum of raw content.
	};

	struct ArchiveInfo
	{
		std::string filePath; // !<Path on disk to binary res2h archive or to the file the archive is embedded in.
		T offsetInFile = 0; // !<Offset of the start of the archive in the file (> 0 when an archive is embedded e.g. in an executable).
		uint32_t fileVersion = 0; // !<File format version (currently 2).
		uint32_t formatFlags = 0; // !<File option flags.
		uint8_t bits = 0; // !<Archive bit depth (32/64).
		T size = 0; // !<Overall size of archive data.
		T checksum = 0; // !<Fletcher-32/64 archive checksum.
		std::vector<ResourceInfo> resources; // !<The list of all resources in this archive.
	};

	/*!
	Return an instance of the singleton Res2h object.
	\return The Res2h object.
	*/
	static Res2h & instance();

	/*!
	Try to find archive header in an archive file or an embedded archive.
	\param[in] archivePath Archive path.
	\return Returns offset if archive can be opened and its magic bytes header is found. Throws an exception otherwise.
	*/
	T findArchiveStartOffset(const std::string & archivePath) const;

	/*!
	Try to read archive header from an archive file or an embedded archive.
	\param[in] archivePath Archive path.
	\param[in] magicOffset Offset to magic bytes.
	\return Returns archive info if archive can be opened and information read properly. Throws an exception otherwise.
	*/
	ArchiveInfo getArchiveInfo(const std::string & archivePath) const;

	/*!
	Open archive file or file with embedded archive from disk and load directory into memory. 
	You can add as many archives as you want. This does NOT load the actual data yet, only the directory.
	\param[in] archivePath Archive path.
	\note Single archives can be appended to other files using res2h: "res2h <ARCHIVE> <APPEND_TARGET> -a"
	\return Returns true if opening and loading the archive directory worked.
	*/
	bool loadArchive(const std::string & archivePath);

	/*!
	Load file content. Can be either a file on disk or a file in a binary archive.
	\param[in] filePath Path to the file. If it start with ":/" it is considered to be in a binary archive.
	\param[in] keepInCache Optional. Pass true to keep the resource in memory if you need it more than once. Memory archive data is never cached, because it is already in memory.
	\param[in] checkChecksum Optional. Pass true to check the calculated checksum of the data against the checksum stored in the archive.
	\return Returns a struct containing the data or throws an exception if it fails to do so.
	\note When loading data from a binary archive, you must load the archive with \loadArchive before.
	*/
	ResourceInfo loadResource(const std::string & filePath, bool keepInCache = false, bool checkChecksum = true);

	/*!
	Return information about all resources on disk and in archive, loaded or not.
	\return Returns information about all resources on disk and in archive, loaded or not.
	*/
	std::vector<ResourceInfo> getResourceInfo() const;

	/*!
	Release all cached data. Keeps directories in memory.
	\note This releases the shared_ptr to the data. If you keep more instances of that shared_ptr, memory will NOT be freed!
	*/
	void releaseCache();

private:
	// !<Load a resource from disk.
	ResourceInfo loadResourceFromDisk(const std::string & filePath);
	// !<Load a resource from a binary archive.
	ResourceInfo loadResourceFromArchive(const ResourceInfo & entry, const ArchiveInfo & archive, bool checkChecksum);

	// !<Cache holding the archive entries.
	std::vector<ArchiveInfo> m_archives;
	// !<Cache holding the on-disk resources.
	std::vector<ResourceInfo> m_diskResources;
};

// -----------------------------------------------------------------------------

template<typename T>
bool Res2h<T>::loadArchive(const std::string & archivePath)
{
	// check if there are entries for this archive already in the map and delete them
	auto aIt = m_archives.begin();
	while (aIt != m_archives.end())
	{
		if (aIt->filePath == archivePath)
		{
			// 'tis from this archive. erase and reload info
			aIt = m_archives.erase(aIt);
		}
		else
		{
			++aIt;
		}
	}
	// try to find archive in file. this will throw if it fails
	ArchiveInfo info = getArchiveInfo(archivePath);
	// open archive
	std::ifstream inStream;
	inStream.open(archivePath, std::ifstream::in | std::ifstream::binary);
	if (inStream.is_open() && inStream.good())
	{
		// file version ok. skip currently unused flags and archive size
		inStream.seekg(info.offsetInFile + (info.bits == 64 ? RES2H_OFFSET_NO_OF_FILES_64 : RES2H_OFFSET_NO_OF_FILES_32));
		// read number of directory entries
		uint32_t nrOfDirectoryEntries = 0;
		inStream.read(reinterpret_cast<char *>(&nrOfDirectoryEntries), sizeof(uint32_t));
		if (nrOfDirectoryEntries > 0)
		{
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
				// clear data pointer
				temp.data = nullptr;
				// skip currently unused flags
				inStream.seekg(sizeof(uint32_t), std::ios::cur);
				// read size of data
				inStream.read(reinterpret_cast<char *>(&temp.dataSize), (info.bits == 64 ? sizeof(uint64_t) : sizeof(uint32_t)));
				// read offset to start of data
				inStream.read(reinterpret_cast<char *>(&temp.dataOffset), (info.bits == 64 ? sizeof(uint64_t) : sizeof(uint32_t)));
				// read data checksum
				inStream.read(reinterpret_cast<char *>(&temp.checksum), (info.bits == 64 ? sizeof(uint64_t) : sizeof(uint32_t)));
				// add to resources
				info.resources.push_back(temp);
			}
			// close file
			inStream.close();
			m_archives.push_back(info);
			return true;
		}
		
		
			inStream.close();
			throw Res2hException(std::string("Archive \"") + archivePath + "\" contains no files!");
		
	}
	else
	{
		throw Res2hException(std::string("Failed to open archive \"") + archivePath + "\" for reading.");
	}
	return false;
}

template<typename T>
typename Res2h<T>::ResourceInfo Res2h<T>::loadResource(const std::string & filePath, bool keepInCache, bool checkChecksum)
{
	ResourceInfo temp;
	// check if from archive or disk
	const bool fromArchive = filePath.find_first_of(":/") == 0;
	if (fromArchive)
	{
		// find file in the archive
		for (auto & archive : m_archives)
		{
			for (auto & resource : archive.resources)
			{
				if (resource.filePath == filePath)
				{
					// file found. check if data is in memory
					if (resource.data)
					{
						return resource;
					}
					
					
						// no. load data first
						auto tempEntry = loadResourceFromArchive(resource, archive, checkChecksum);
						if (keepInCache)
						{
							resource = tempEntry;
						}
						return tempEntry;
					
				}
			}
		}
	}
	else
	{
		// find file in the disk resources list
		for (auto & resource : m_diskResources)
		{
			if (resource.filePath == filePath)
			{
				// file found. check if data is in memory
				if (resource.data)
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
	}
	// when we get here the file was not in any archive
	if (fromArchive)
	{
		// archive file. we can't load files from archives we have no directory for. notify caller.
		throw Res2hException(std::string("File \"") + filePath + "\" is unknown. Please use loadArchive() first.");
	}
	throw Res2hException(std::string("Failed to load file \"") + filePath + "\".");
}

template<typename T>
typename Res2h<T>::ResourceInfo Res2h<T>::loadResourceFromDisk(const std::string & filePath)
{
	ResourceInfo temp;
	// try to open file
	std::ifstream inStream;
	inStream.open(filePath, std::ifstream::in | std::ifstream::binary);
	if (inStream.is_open() && inStream.good())
	{
		// opened ok. check file size
		inStream.seekg(0, std::ios::end);
		size_t fileSize = (size_t)inStream.tellg();
		inStream.seekg(0);
		if (fileSize > 0)
		{
			// allocate data
			std::shared_ptr<unsigned char> fileData = std::shared_ptr<unsigned char>(new unsigned char[fileSize], [](const unsigned char *p) { delete[] p; });
			// try reading data
			try
			{
				inStream.read(reinterpret_cast<char *>(fileData.get()), fileSize);
			}
			catch (std::ios_base::failure) { /*reading didn't work properly. salvage what we can.*/ }
			// check how many bytes were actually read
			if (inStream.gcount() != fileSize)
			{
				throw Res2hException(std::string("Failed to read file \"") + filePath + "\".");
			}
			// seems to have worked. store data.
			temp.filePath = filePath;
			temp.data = fileData;
			temp.dataSize = fileSize;
		}
	}
	else
	{
		throw Res2hException(std::string("Failed to open file \"") + filePath + "\" for reading.");
	}
	return temp;
}

template<typename T>
typename Res2h<T>::ResourceInfo Res2h<T>::loadResourceFromArchive(const ResourceInfo & entry, const ArchiveInfo & archive, bool checkChecksum)
{
	ResourceInfo temp = entry;
	// try to open archive file
	std::ifstream inStream;
	inStream.open(archive.filePath, std::ifstream::in | std::ifstream::binary);
	if (inStream.is_open() && inStream.good())
	{
		// opened ok. move to data offset
		inStream.seekg(archive.offsetInFile + temp.dataOffset);
		// allocate data
		temp.data = std::shared_ptr<unsigned char>(new unsigned char[temp.dataSize], [](const unsigned char *p) { delete[] p; });
		// try reading data
		try
		{
			inStream.read(reinterpret_cast<char *>(temp.data.get()), temp.dataSize);
		}
		catch (std::ios_base::failure) { /*reading didn't work properly. salvage what we can.*/ }
		// check how many bytes were actually read
		if (inStream.gcount() != temp.dataSize)
		{
			throw Res2hException(std::string("Failed to read \"") + temp.filePath + "\" from archive.");
		}
		//  now that we're here, do a checksum
		if (checkChecksum)
		{
			T dataCheckum = (archive.bits == 64 ? calculateFletcher<uint64_t>(temp.data.get(), temp.dataSize) : calculateFletcher<uint32_t>(temp.data.get(), static_cast<uint32_t>(temp.dataSize)));
			if (temp.checksum != dataCheckum)
			{
				throw Res2hException(std::string("Failed to read \"") + temp.filePath + "\" from archive. Bad checksum!");
			}
		}
	}
	else
	{
		throw Res2hException(std::string("Failed to open archive \"") + archive.filePath + "\" for reading.");
	}
	return temp;
}

template<typename T>
std::vector<typename Res2h<T>::ResourceInfo> Res2h<T>::getResourceInfo() const
{
	std::vector<Res2h::ResourceInfo> result;
	for (auto & archive : m_archives)
	{
		for (auto & resource : archive.resources)
		{
			result.push_back(resource);
		}
	}
	for (auto & resource : m_diskResources)
	{
		result.push_back(resource);
	}
	return result;
}
