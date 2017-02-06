#include "res2hinterface.h"

#include <climits>

template<>
Res2h<uint32_t> & Res2h<uint32_t>::instance()
{
	static Res2h<uint32_t> instance;
	return instance;
}

template<>
Res2h<uint64_t> & Res2h<uint64_t>::instance()
{
	static Res2h<uint64_t> instance;
	return instance;
}

template<typename T>
T Res2h<T>::findArchiveStartOffset(const std::string & archivePath) const
{
	// check if the archive is in our list already
	for (auto & archive : m_archives)
	{
		if (archive.filePath == archivePath)
		{
			return archive.offsetInFile;
		}
	}
	// no. open archive
	std::ifstream inStream;
	inStream.open(archivePath, std::ifstream::in | std::ifstream::binary);
	if (inStream.is_open() && inStream.good())
	{
		// try to read magic bytes
		unsigned char magicBytes[8] = {0};
		inStream.read(reinterpret_cast<char *>(&magicBytes), sizeof(magicBytes));
		std::string magicString(reinterpret_cast<char *>(&magicBytes), sizeof(magicBytes));
		if (magicString == RES2H_MAGIC_BYTES)
		{
			// found. close and return offset
			inStream.close();
			return 0;
		}
		else
		{
			// no magic bytes at start. might be an embedded archive, search for a header backwards from EOF...
			unsigned char buffer[4096] = {0};
			// seek to end minus buffer size
			inStream.seekg(sizeof(buffer), std::ios::end);
			// read whole file to search for archive
			while (inStream.good())
			{
				// read block of data and convert to string
				inStream.read(reinterpret_cast<char *>(&buffer), sizeof(buffer));
				std::string magicString(reinterpret_cast<char *>(&buffer), sizeof(buffer));
				// try to find magic bytes
				std::streamoff magicPosition = magicString.find(RES2H_MAGIC_BYTES);
				if (magicPosition != std::string::npos)
				{
					// found. close and return offset
					inStream.close();
					return static_cast<T>(inStream.tellg() + magicPosition);
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
		}
		// close file. nothing found.
		inStream.close();
	}
	else
	{
		throw Res2hException(std::string("Failed to open archive \"") + archivePath + "\" for reading.");
	}
	throw Res2hException(std::string("No valid archive found in \"") + archivePath + "\".");
}

template<>
Res2h<uint32_t>::ArchiveInfo Res2h<uint32_t>::getArchiveInfo(const std::string & archivePath) const
{
	// check if the archive is in our list already
	for (auto & archive : m_archives)
	{
		if (archive.filePath == archivePath)
		{
			return archive;
		}
	}
	// no. try to find archive start in file. If it is not found it will throw. we don't catch the exception here, but simply pass it on...
	ArchiveInfo info;
	info.filePath = archivePath;
	info.offsetInFile = findArchiveStartOffset(archivePath);
	// open archive again
	std::ifstream inStream;
	inStream.open(archivePath, std::ifstream::in | std::ifstream::binary);
	if (inStream.is_open() && inStream.good())
	{
		// nice. magic bytes ok. read file version
		inStream.seekg(info.offsetInFile + RES2H_OFFSET_FILE_VERSION);
		inStream.read(reinterpret_cast<char *>(&info.fileVersion), sizeof(uint32_t));
		if (info.fileVersion != RES2H_ARCHIVE_VERSION)
		{
			inStream.close();
			throw Res2hException(std::string("Bad archive file version " + std::to_string(info.fileVersion) + " in \"") + archivePath + "\".");
		}
		// check file flags (32/64 bit)
		inStream.seekg(info.offsetInFile + RES2H_OFFSET_FORMAT_FLAGS);
		inStream.read(reinterpret_cast<char *>(&info.formatFlags), sizeof(uint32_t));
		// the low 8 bit of the flags is the archive bit depth, e.g. 32/64 bit
		info.bits = info.formatFlags & 0x000000FF;
		// now try to read header depending on the bit depth
		if (info.bits == 32)
		{
			// get size of the whole archive.
			uint32_t archiveSize = 0;
			inStream.seekg(info.offsetInFile + RES2H_OFFSET_ARCHIVE_SIZE);
			inStream.read(reinterpret_cast<char *>(&archiveSize), sizeof(uint32_t));
			info.size = archiveSize;
			if (info.size <= 0)
			{
				inStream.close();
				throw Res2hException(std::string("Archive \"") + archivePath + "\" has an internal size of 0.");
			}
			// read checksum from end of file
			uint32_t fileChecksum = 0;
			inStream.seekg(info.offsetInFile + info.size - sizeof(uint32_t));
			inStream.read(reinterpret_cast<char *>(&fileChecksum), sizeof(uint32_t));
			info.checksum = fileChecksum;
			// control checksum. close first for calling checksum function
			inStream.close();
			if (info.checksum != calculateFletcher<uint32_t>(archivePath, info.size - sizeof(uint32_t)))
			{
				throw Res2hException(std::string("Archive \"") + archivePath + "\" has a bad checksum.");
			}
			return info;
		}
		else if (info.bits == 64)
		{
			inStream.close();
			throw Res2hException(std::string("Can not read 64 bit archive \"") + archivePath + "\". Only 32 bit archives can be read");
		}
		else
		{
			inStream.close();
			throw Res2hException(std::string("Bad archive bit depth of " + std::to_string(info.bits) + " in \"") + archivePath + "\".");
		}
	}
	else
	{
		throw Res2hException(std::string("Failed to open archive \"") + archivePath + "\" for reading.");
	}
}

template<>
Res2h<uint64_t>::ArchiveInfo Res2h<uint64_t>::getArchiveInfo(const std::string & archivePath) const
{
	// check if the archive is in our list already
	for (auto & archive : m_archives)
	{
		if (archive.filePath == archivePath)
		{
			return archive;
		}
	}
	// no. try to find archive start in file. If it is not found it will throw. we don't catch the exception here, but simply pass it on...
	ArchiveInfo info;
	info.filePath = archivePath;
	info.offsetInFile = findArchiveStartOffset(archivePath);
	// open archive again
	std::ifstream inStream;
	inStream.open(archivePath, std::ifstream::in | std::ifstream::binary);
	if (inStream.is_open() && inStream.good())
	{
		// nice. magic bytes ok. read file version
		inStream.seekg(info.offsetInFile + RES2H_OFFSET_FILE_VERSION);
		inStream.read(reinterpret_cast<char *>(&info.fileVersion), sizeof(uint32_t));
		if (info.fileVersion != RES2H_ARCHIVE_VERSION)
		{
			inStream.close();
			throw Res2hException(std::string("Bad archive file version " + std::to_string(info.fileVersion) + " in \"") + archivePath + "\".");
		}
		// check file flags (32/64 bit)
		inStream.seekg(info.offsetInFile + RES2H_OFFSET_FORMAT_FLAGS);
		inStream.read(reinterpret_cast<char *>(&info.formatFlags), sizeof(uint32_t));
		// the low 8 bit of the flags is the archive bit depth, e.g. 32/64 bit
		info.bits = info.formatFlags & 0x000000FF;
		if (info.bits != 32 && info.bits != 64)
		{
			inStream.close();
			throw Res2hException(std::string("Bad archive bit depth of " + std::to_string(info.bits) + " in \"") + archivePath + "\".");
		}
		// get size of the whole archive.
		uint64_t archiveSize = 0;
		inStream.seekg(info.offsetInFile + RES2H_OFFSET_ARCHIVE_SIZE);
		inStream.read(reinterpret_cast<char *>(&archiveSize), (info.bits == 64 ? sizeof(uint64_t) : sizeof(uint32_t)));
		info.size = archiveSize;
		if (info.size <= 0)
		{
			inStream.close();
			throw Res2hException(std::string("Archive \"") + archivePath + "\" has an internal size of 0.");
		}
		// read checksum from end of file
		uint64_t readChecksum = 0;
		inStream.seekg(info.offsetInFile + info.size - (info.bits == 64 ? sizeof(uint64_t) : sizeof(uint32_t)));
		inStream.read(reinterpret_cast<char *>(&readChecksum), (info.bits == 64 ? sizeof(uint64_t) : sizeof(uint32_t)));
		info.checksum = readChecksum;
		// control checksum. close first for calling checksum function
		inStream.close();
		uint64_t fileChecksum = (info.bits == 64 ? calculateFletcher<uint64_t>(archivePath, info.size - sizeof(uint64_t)) : calculateFletcher<uint32_t>(archivePath, static_cast<uint32_t>(info.size - sizeof(uint32_t))));
		if (info.checksum != fileChecksum)
		{
			throw Res2hException(std::string("Archive \"") + archivePath + "\" has a bad checksum.");
		}
		return info;
	}
	else
	{
		throw Res2hException(std::string("Failed to open archive \"") + archivePath + "\" for reading.");
	}
}

template<typename T>
void Res2h<T>::releaseCache()
{
	// erase all allocated data in resource map
	for (auto & archive : m_archives)
	{
		for (auto & resource : archive.resources)
		{
			// shared_ptr is valid, so there is allocated data. release our reference to it.
			resource.data.reset();
		}
	}
}

// -----------------------------------------------------------------------------

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
