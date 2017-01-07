#pragma once

#include <cstdint>
#include <string>
#include <memory>
#include <map>
#include <exception>

#include "res2h.h"

/*
Exceptions thrown by Res2h when something goes wrong.
*/
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

//-----------------------------------------------------------------------------

/*
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
	struct ResourceEntry
	{
		std::string filePath; //!<Name of file. If it starts with ":/" it is considered an internal file in a binary res2h archive.
		std::shared_ptr<uint8_t> data; //!<Raw file content.
		T dataSize = 0; //!<Raw content size.
		T dataOffset = 0; //!<Raw content offset in binary res2h archive if any (Start of data = offsetInFile + dataOffset).
		T checksum = 0; //!<Fletcher-32/64 Checksum of raw content.
		std::string archivePath; //!<Path on disk to binary res2h archive or to the file the archive is embedded in.
		T offsetInFile = 0; //!<Offset of the archive data in the file (> 0 when an archive is embedded e.g. in an executable).
	};

	struct ArchiveInfo
	{
		bool valid = false;
		std::string archivePath; //!<Path on disk to binary res2h archive or to the file the archive is embedded in.
		T offsetInFile = 0; //!<Offset of the start of the archive in the file (> 0 when an archive is embedded e.g. in an executable).
		uint32_t fileVersion = 0; //!<File format version (currently 2).
		uint32_t formatFlags = 0; //!<File option flags.
		uint8_t bits = 0; //!<Archive bit depth (32/64).
		T size = 0; //!<Overall size of archive data.
		T checksum = 0; //!<Archive checksum.
	};

	/*
	Return an instance of the singleton Res2h object.
	\return The Res2h object.
	*/
	static Res2h & getInstance() { static Res2h<T> instance;	return instance; }

	/*
	Try to find archive header in an archive file or an embedded archive.
	\param[in] archivePath Archive path.
	\return Returns offset if archive can be opened and its magic bytes header is found. Throws an exception otherwise.
	*/
	T findArchiveStartOffset(const std::string & archivePath);

	/*
	Try to read archive header from an archive file or an embedded archive.
	\param[in] archivePath Archive path.
	\param[in] magicOffset Offset to magic bytes.
	\return Returns archive info if archive can be opened and information read properly. Throws an exception otherwise.
	*/
	ArchiveInfo readArchiveInfo(const std::string & archivePath);

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
	ResourceEntry loadFile(const std::string & filePath, bool keepInCache = false, bool checkChecksum = true);

	/*!
	Return the number of resources currently in the map.
	\return Returns the number of resources currently loaded.
	*/
	uint32_t getNrOfResources();

	/*!
	Return a resource from the map.
	\param[in] index The index of the resource to return.
	\return Returns the index'th resource from the map.
	*/
	ResourceEntry getResource(uint32_t index);

	/*!
	Release all cached data. Keeps directories in memory.
	\note This releases the shared_ptr to the data. If you keep more instances of that shared_ptr, memory will NOT be freed!
	*/
	void releaseCache();

private:
	//!<Cache holding the resource entries.
	std::map<std::string, ResourceEntry> m_resourceMap;
	//!<Load a file from disk.
	ResourceEntry loadFileFromDisk(const std::string & filePath);
	//!<Load a file from a binary archive.
	ResourceEntry loadFileFromArchive(const ResourceEntry & entry);
};

//-----------------------------------------------------------------------------

template<typename T>
bool Res2h<T>::loadArchive(const std::string & archivePath)
{
	//make hash from archive path
	T pathHash = calculateFletcher<uint32_t>(archivePath);
	//check if there are entries for this archive already in the map and delete them
	auto rmIt = m_resourceMap.begin();
	while (rmIt != m_resourceMap.end())
	{
		if (rmIt->second.archivePath == archivePath)
		{
			//'tis from this archive. erase.
			rmIt = m_resourceMap.erase(rmIt);
		}
		else
		{
			++rmIt;
		}
	}
	//try to find archive in file
	ArchiveInfo info;
	try
	{
		info = readArchiveInfo(archivePath);
	}
	catch (Res2hException e)
	{
		return false;
	}
	//open archive
	std::ifstream inStream;
	inStream.open(archivePath, std::ifstream::in | std::ifstream::binary);
	if (inStream.is_open() && inStream.good())
	{
		//file version ok. skip currently unused flags and archive size
		inStream.seekg(info.offsetInFile + info.bits == 64 ? RES2H_OFFSET_NO_OF_FILES_64 : RES2H_OFFSET_NO_OF_FILES_32);
		//read number of directory entries
		uint32_t nrOfDirectoryEntries = 0;
		inStream.read(reinterpret_cast<char *>(&nrOfDirectoryEntries), sizeof(uint32_t));
		if (nrOfDirectoryEntries > 0)
		{
			for (uint32_t i = 0; i < nrOfDirectoryEntries; ++i)
			{
				//read directory to map
				ResourceEntry temp;
				//read size of name
				uint32_t sizeOfName = 0;
				inStream.read(reinterpret_cast<char *>(&sizeOfName), sizeof(uint32_t));
				//read name itself
				std::string filePath;
				filePath.resize(sizeOfName);
				inStream.read(reinterpret_cast<char *>(&filePath[0]), sizeOfName - 1);
				//skip the null-terminator
				inStream.seekg(1, std::ios::cur);
				//clear data pointer
				temp.data = nullptr;
				//skip currently unused flags
				inStream.seekg(sizeof(uint32_t), std::ios::cur);
				//read size of data
				inStream.read(reinterpret_cast<char *>(&temp.dataSize), sizeof(T));
				//read offset to start of data
				inStream.read(reinterpret_cast<char *>(&temp.dataOffset), sizeof(T));
				//read data checksum
				inStream.read(reinterpret_cast<char *>(&temp.checksum), sizeof(T));
				//add archive path and offset
				temp.archivePath = archivePath;
				temp.offsetInFile = info.offsetInFile;
				//add to map
				m_resourceMap[filePath] = temp;
			}
			//close file
			inStream.close();
			return true;
		}
		else
		{
			inStream.close();
			throw Res2hException(std::string("loadArchive() - Archive \"") + archivePath + "\" contains no files!");
		}
	}
	else
	{
		throw Res2hException(std::string("loadArchive() - Failed to open archive \"") + archivePath + "\" for reading.");
	}
	return false;
}

template<typename T>
typename Res2h<T>::ResourceEntry Res2h<T>::loadFile(const std::string & filePath, bool keepInCache, bool checkChecksum)
{
	ResourceEntry temp;
	//check if from archive
	bool fromArchive = filePath.find_first_of(":/") == 0;
	//find file in the map
	auto rmIt = m_resourceMap.find(filePath);
	if (rmIt != m_resourceMap.end())
	{
		//found. check if still in memory.
		temp = rmIt->second;
		if (temp.data)
		{
			//data is still valid. return it.
			return temp;
		}
		//data needs to be loaded. check if archive file
		if (fromArchive)
		{
			//yes. try to load archive
			temp = loadFileFromArchive(temp);
			//calculate checksum if user wants to
			if (checkChecksum)
			{
				if (temp.checksum != calculateFletcher<uint64_t>(temp.data.get(), temp.dataSize))
				{
					throw Res2hException(std::string("loadFile() - Bad checksum for \"") + filePath + "\".");
				}
			}
		}
		else
		{
			//try to open disk file
			temp = loadFileFromDisk(temp.filePath);
		}
		//loaded. if the user wants to cache the data, add it to our map, else just return it and it will be freed eventually
		if (keepInCache)
		{
			rmIt->second.data = temp.data;
		}
		return temp;
	}
	//when we get here the file was not in the map. try to load it
	if (fromArchive)
	{
		//archive file. we can't load files from archives we have no directory for. notify caller.
		throw Res2hException(std::string("loadFile() - File \"") + filePath + "\" is unkown. Please use loadArchive() first.");
	}
	else
	{
		//try to open disk file
		temp = loadFileFromDisk(temp.filePath);
		//loaded. if the user wants to cache the data, add it to our map, else just return it and it will be freed eventually
		if (keepInCache)
		{
			m_resourceMap[temp.filePath] = temp;
		}
	}
	return temp;
}

template<typename T>
typename Res2h<T>::ResourceEntry Res2h<T>::loadFileFromDisk(const std::string & filePath)
{
	ResourceEntry temp;
	//try to open file
	std::ifstream inStream;
	inStream.open(filePath, std::ifstream::in | std::ifstream::binary);
	if (inStream.is_open() && inStream.good())
	{
		//opened ok. check file size
		inStream.seekg(0, std::ios::end);
		size_t fileSize = (size_t)inStream.tellg();
		inStream.seekg(0);
		if (fileSize > 0)
		{
			//allocate data
			std::shared_ptr<unsigned char> fileData = std::shared_ptr<unsigned char>(new unsigned char[fileSize], [](unsigned char *p) { delete[] p; });
			//try reading data
			try
			{
				inStream.read(reinterpret_cast<char *>(fileData.get()), fileSize);
			}
			catch (std::ios_base::failure) { /*reading didn't work properly. salvage what we can.*/ }
			//check how many bytes were actually read
			if (inStream.gcount() != fileSize)
			{
				throw Res2hException(std::string("loadFileFromDisk() - Failed to read file \"") + filePath + "\".");
			}
			//seems to have worked. store data.
			temp.filePath = filePath;
			temp.data = fileData;
			temp.dataSize = fileSize;
		}
	}
	else
	{
		throw Res2hException(std::string("loadFileFromDisk() - Failed to open file \"") + filePath + "\" for reading.");
	}
	return temp;
}

template<typename T>
typename Res2h<T>::ResourceEntry Res2h<T>::loadFileFromArchive(const ResourceEntry & entry)
{
	ResourceEntry temp = entry;
	//try to open archive file
	std::ifstream inStream;
	inStream.open(temp.archivePath, std::ifstream::in | std::ifstream::binary);
	if (inStream.is_open() && inStream.good())
	{
		//opened ok. move to data offset
		inStream.seekg(temp.offsetInFile + temp.dataOffset);
		//allocate data
		temp.data = std::shared_ptr<unsigned char>(new unsigned char[temp.dataSize], [](unsigned char *p) { delete[] p; });
		//try reading data
		try
		{
			inStream.read(reinterpret_cast<char *>(temp.data.get()), temp.dataSize);
		}
		catch (std::ios_base::failure) { /*reading didn't work properly. salvage what we can.*/ }
		//check how many bytes were actually read
		if (inStream.gcount() != temp.dataSize)
		{
			throw Res2hException(std::string("loadFileFromArchive() - Failed to read \"") + temp.filePath + "\" from archive.");
		}
	}
	else
	{
		throw Res2hException(std::string("loadFileFromArchive() - Failed to open archive \"") + temp.archivePath + "\" for reading.");
	}
	return temp;
}

template<typename T>
uint32_t Res2h<T>::getNrOfResources()
{
	return static_cast<uint32_t>(m_resourceMap.size());
}

template<typename T>
typename Res2h<T>::ResourceEntry Res2h<T>::getResource(uint32_t index)
{
	if (index < m_resourceMap.size())
	{
		auto rmIt = m_resourceMap.cbegin();
		advance(rmIt, index);
		if (rmIt != m_resourceMap.end())
		{
			return rmIt->second;
		}
		else
		{
			throw Res2hException(std::string("getFile() - Index ") + std::to_string((long long)index) + " out of bounds! Map has " + std::to_string((long long)m_resourceMap.size()) + "files.");
		}
	}
	else
	{
		throw Res2hException(std::string("getFile() - Index ") + std::to_string((long long)index) + " out of bounds! Map has " + std::to_string((long long)m_resourceMap.size()) + "files.");
	}
}
