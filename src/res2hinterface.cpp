#include "res2hinterface.hpp"

#include <climits>
#include <fstream>

#include "checksum.hpp"


template<typename T>
T Res2h<T>::findArchiveStartOffset(const std::string & archivePath)
{
	//open archive
	std::ifstream inStream;
	inStream.open(archivePath, std::ifstream::in | std::ifstream::binary);
	if (inStream.is_open() && inStream.good())
	{
		//try to read magic bytes
		unsigned char magicBytes[8] = {0};
		inStream.read(reinterpret_cast<char *>(&magicBytes), sizeof(magicBytes));
		std::string magicString(reinterpret_cast<char *>(&magicBytes), sizeof(magicBytes));
		if (magicString == RES2H_MAGIC_BYTES)
		{
			//found. close and return offset
			inStream.close();
			return 0;
		}
		else
		{
			//no magic bytes at start. might be an embedded archive, search for a header backwards from EOF...
			unsigned char buffer[4096] = {0};
			//seek to end minus buffer size
			inStream.seekg(sizeof(buffer), std::ios::end);
			//read whole file to search for archive
			while (inStream.good())
			{
				//read block of data and convert to string
				inStream.read(reinterpret_cast<char *>(&buffer), sizeof(buffer));
				std::string magicString(reinterpret_cast<char *>(&buffer), sizeof(buffer));
				//try to find magic bytes
				std::streamoff magicPosition = magicString.find(RES2H_MAGIC_BYTES);
				if (magicPosition != std::string::npos)
				{
					//found. close and return offset
					inStream.close();
					return static_cast<T>(inStream.tellg() + magicPosition);
				}
				//check if we're already at the start of the stream
				if (inStream.tellg() == std::streampos(0))
				{
					break;
				}
				//no. seek the size of the buffer in direction of the start of the file, 
				//but read some bytes again, else we could miss the header in between buffer 
				inStream.seekg(-static_cast<int64_t>(sizeof(buffer) - sizeof(RES2H_MAGIC_BYTES) + 1), std::ios::cur);
			}
		}
		//close file. nothing found.
		inStream.close();
	}
	else
	{
		throw Res2hException(std::string("findArchiveStartOffset() - Failed to open archive \"") + archivePath + "\" for reading.");
	}
	throw Res2hException(std::string("findArchiveStartOffset() - No valid archive found in \"") + archivePath + "\".");
}

template<>
Res2h<uint32_t>::ArchiveInfo Res2h<uint32_t>::readArchiveInfo(const std::string & archivePath)
{
	//try to find archive start in file. If it is not found it will throw. we don't catch the exception here, but simply pass it on...
	ArchiveInfo info;
	info.offsetInFile = findArchiveStartOffset(archivePath);
	//open archive again
	std::ifstream inStream;
	inStream.open(archivePath, std::ifstream::in | std::ifstream::binary);
	if (inStream.is_open() && inStream.good())
	{
		//nice. magic bytes ok. read file version
		inStream.seekg(info.offsetInFile + RES2H_OFFSET_FILE_VERSION);
		inStream.read(reinterpret_cast<char *>(&info.fileVersion), sizeof(uint32_t));
		if (info.fileVersion != RES2H_ARCHIVE_VERSION)
		{
			inStream.close();
			throw Res2hException(std::string("readArchiveInfo() - Bad archive file version " + std::to_string(info.fileVersion) + " in \"") + archivePath + "\".");
		}
		//check file flags (32/64 bit)
		inStream.seekg(info.offsetInFile + RES2H_OFFSET_FORMAT_FLAGS);
		inStream.read(reinterpret_cast<char *>(&info.formatFlags), sizeof(uint32_t));
		//the low 8 bit of the flags is the archive bit depth, e.g. 32/64 bit
		info.bits = info.formatFlags & 0x000000FF;
		//now try to read header depending on the bit depth
		if (info.bits == 32)
		{
			//get size of the whole archive.
			uint32_t archiveSize = 0;
			inStream.seekg(info.offsetInFile + RES2H_OFFSET_ARCHIVE_SIZE);
			inStream.read(reinterpret_cast<char *>(&archiveSize), sizeof(uint32_t));
			info.size = archiveSize;
			if (info.size > 0)
			{
				inStream.close();
				throw Res2hException(std::string("readArchiveInfo() - Archive \"") + archivePath + "\" has a size of 0.");
			}
			//read checksum from end of file
			uint32_t fileChecksum = 0;
			inStream.open(archivePath, std::ifstream::in | std::ifstream::binary);
			inStream.seekg(info.offsetInFile + info.size - sizeof(uint32_t));
			inStream.read(reinterpret_cast<char *>(&fileChecksum), sizeof(uint32_t));
			info.checksum = fileChecksum;
			//control checksum. close first for calling checksum function
			inStream.close();
			if (info.checksum != calculateFletcher<uint32_t>(archivePath, info.size - sizeof(uint32_t)))
			{
				throw Res2hException(std::string("readArchiveInfo() - Archive \"") + archivePath + "\" has a bad checksum.");
			}
			info.valid = true;
			return info;
		}
		else if (info.bits == 64)
		{
			inStream.close();
			throw Res2hException(std::string("readArchiveInfo() - Can not read archive \"") + archivePath + "\". Only 32 bit archives can be read");
		}
		else
		{
			inStream.close();
			throw Res2hException(std::string("readArchiveInfo() - Bad archive bith depth of " + std::to_string(info.bits) + " in \"") + archivePath + "\".");
		}
	}
	else
	{
		throw Res2hException(std::string("readArchiveInfo() - Failed to open archive \"") + archivePath + "\" for reading.");
	}
}

template<>
Res2h<uint64_t>::ArchiveInfo Res2h<uint64_t>::readArchiveInfo(const std::string & archivePath)
{
	//try to find archive start in file. If it is not found it will throw. we don't catch the exception here, but simply pass it on...
	ArchiveInfo info;
	info.offsetInFile = findArchiveStartOffset(archivePath);
	//open archive again
	std::ifstream inStream;
	inStream.open(archivePath, std::ifstream::in | std::ifstream::binary);
	if (inStream.is_open() && inStream.good())
	{
		//nice. magic bytes ok. read file version
		inStream.seekg(info.offsetInFile + RES2H_OFFSET_FILE_VERSION);
		inStream.read(reinterpret_cast<char *>(&info.fileVersion), sizeof(uint32_t));
		if (info.fileVersion != RES2H_ARCHIVE_VERSION)
		{
			inStream.close();
			throw Res2hException(std::string("readArchiveInfo() - Bad archive file version " + std::to_string(info.fileVersion) + " in \"") + archivePath + "\".");
		}
		//check file flags (32/64 bit)
		inStream.seekg(info.offsetInFile + RES2H_OFFSET_FORMAT_FLAGS);
		inStream.read(reinterpret_cast<char *>(&info.formatFlags), sizeof(uint32_t));
		//the low 8 bit of the flags is the archive bit depth, e.g. 32/64 bit
		info.bits = info.formatFlags & 0x000000FF;
		//now try to read header depending on the bit depth
		if (info.bits == 32)
		{
			//get size of the whole archive.
			uint32_t archiveSize = 0;
			inStream.seekg(info.offsetInFile + RES2H_OFFSET_ARCHIVE_SIZE);
			inStream.read(reinterpret_cast<char *>(&archiveSize), sizeof(uint32_t));
			info.size = archiveSize;
			if (info.size > 0)
			{
				inStream.close();
				throw Res2hException(std::string("readArchiveInfo() - Archive \"") + archivePath + "\" has a size of 0.");
			}
			//read checksum from end of file
			uint32_t fileChecksum = 0;
			inStream.open(archivePath, std::ifstream::in | std::ifstream::binary);
			inStream.seekg(info.offsetInFile + info.size - sizeof(uint32_t));
			inStream.read(reinterpret_cast<char *>(&fileChecksum), sizeof(uint32_t));
			info.checksum = fileChecksum;
			//control checksum. close first for calling checksum function
			inStream.close();
			if (info.checksum != calculateFletcher<uint32_t>(archivePath, info.size - sizeof(uint32_t)))
			{
				throw Res2hException(std::string("readArchiveInfo() - Archive \"") + archivePath + "\" has a bad checksum.");
			}
			info.valid = true;
			return info;
		}
		else if (info.bits == 64)
		{
			//get size of the whole archive.
			inStream.seekg(info.offsetInFile + RES2H_OFFSET_ARCHIVE_SIZE);
			inStream.read(reinterpret_cast<char *>(&info.size), sizeof(uint64_t));
			if (info.size > 0)
			{
				inStream.close();
				throw Res2hException(std::string("readArchiveInfo() - Archive \"") + archivePath + "\" has a size of 0.");
			}
			//read checksum from end of file
			inStream.open(archivePath, std::ifstream::in | std::ifstream::binary);
			inStream.seekg(info.offsetInFile + info.size - sizeof(uint64_t));
			inStream.read(reinterpret_cast<char *>(&info.checksum), sizeof(uint64_t));
			//control checksum. close first for calling checksum function
			inStream.close();
			if (info.checksum != calculateFletcher<uint64_t>(archivePath, info.size - sizeof(uint64_t)))
			{
				throw Res2hException(std::string("readArchiveInfo() - Archive \"") + archivePath + "\" has a bad checksum.");
			}
			info.valid = true;
			return info;
		}
		else
		{
			inStream.close();
			throw Res2hException(std::string("readArchiveInfo() - Bad archive bith depth of " + std::to_string(info.bits) + " in \"") + archivePath + "\".");
		}
	}
	else
	{
		throw Res2hException(std::string("readArchiveInfo() - Failed to open archive \"") + archivePath + "\" for reading.");
	}
}

template<typename T>
void Res2h<T>::releaseCache()
{
	//erase all allocated data in resource map
	auto rmIt = m_resourceMap.begin();
	while (rmIt != m_resourceMap.end())
	{
		if (rmIt->second.data)
		{
			//shared_ptr is valid, so there is allocated data. release our reference to it.
			rmIt->second.data.reset();
		}
		++rmIt;
	}
}

//-----------------------------------------------------------------------------

Res2hException::Res2hException(const char * errorString) throw()
	: exception(), error(errorString)
{
}

Res2hException::Res2hException(const std::string & errorString) throw()
	: exception(), error(errorString)
{
}

const char * Res2hException::what() const throw()
{
	return error.c_str();
}

const std::string & Res2hException::whatString() const throw()
{
	return error;
}
