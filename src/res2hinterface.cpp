#include "res2hinterface.hpp"

#include <climits>
#include <fstream>

#include "res2hutils.hpp"


std::map<std::string, Res2h::ResourceEntry> Res2h::resourceMap;


bool Res2h::isArchive(const std::string & archivePath)
{
	//open archive
	std::ifstream inStream;
	inStream.open(archivePath, std::ifstream::in | std::ifstream::binary);
	if (inStream.is_open() && inStream.good()) {
		//try to read magic bytes
		unsigned char magicBytes[8] = {0};
		inStream.read(reinterpret_cast<char *>(&magicBytes), sizeof(magicBytes));
		std::string magicString(reinterpret_cast<char *>(&magicBytes), sizeof(magicBytes));
		if (magicString == RES2H_MAGIC_BYTES) {
			//found. close and return true
			inStream.close();
			return true;
		}
		else {
			//no magic bytes at start. might be an embedded archive, search for a header backwards from EOF...
			bool archiveFound = false;
			//seek to end minus buffer size
			unsigned char buffer[1024] = {0};
			inStream.seekg(sizeof(buffer), std::ios::end);
			//read whole file to search for archive
			while (inStream.tellg() && inStream.good()) {
				//read block of data and convert to string
				inStream.read(reinterpret_cast<char *>(&buffer), sizeof(buffer));
				std::string magicString(reinterpret_cast<char *>(&buffer), sizeof(buffer));
				//try to find magic bytes
				std::streamoff magicPosition = magicString.find(RES2H_MAGIC_BYTES);
				if (magicPosition != std::string::npos) {
					//found.
					archiveFound = true;
					break;
				}
			}
			//close file and return result
			inStream.close();
			return archiveFound;
		}
		//close file. nothing found.
		inStream.close();
	}
	else {
        throw Res2hException(std::string("isArchive() - Failed to open archive \"") + archivePath + "\" for reading.");
    }
	return false;
}

bool Res2h::loadArchive(const std::string & archivePath)
{
    //check if there are entries for this archive already in the map and delete them
    auto rmIt = resourceMap.begin();
    while (rmIt != resourceMap.end()) {
        if (rmIt->second.archivePath == archivePath) {
            //'tis from this archive. erase.
            rmIt = resourceMap.erase(rmIt);
        }
        else {
            ++rmIt;
        }
    }
    //open archive
    std::ifstream inStream;
    inStream.open(archivePath, std::ifstream::in | std::ifstream::binary);
    if (inStream.is_open() && inStream.good()) {
		size_t archiveOffset = 0;
		//try to read magic bytes
		unsigned char magicBytes[sizeof(RES2H_MAGIC_BYTES) - 1] = {0};
		inStream.read(reinterpret_cast<char *>(&magicBytes), sizeof(magicBytes));
		std::string magicString(reinterpret_cast<char *>(&magicBytes), sizeof(magicBytes));
		if (magicString != RES2H_MAGIC_BYTES) {
			//no magic bytes at start. might be an embedded archive, search for a header backwards from EOF...
			archiveOffset = UINT_MAX;
			//seek to end minus buffer size
			unsigned char buffer[1024] = {0};
			inStream.seekg(sizeof(buffer), std::ios::end);
			//read whole file to search for archive
			while (inStream.tellg() && inStream.good()) {
				//read block of data and convert to string
				inStream.read(reinterpret_cast<char *>(&buffer), sizeof(buffer));
				std::string magicString(reinterpret_cast<char *>(&buffer), sizeof(buffer));
				//try to find magic bytes
				std::streamoff magicPosition = magicString.find(RES2H_MAGIC_BYTES);
				if (magicPosition != std::string::npos) {
					//found. move to start position and store position
					inStream.seekg(inStream.tellg() + magicPosition);
					archiveOffset = (size_t)inStream.tellg();
					break;
				}
			}
			//check if the archive was found
			if (archiveOffset == UINT_MAX) {
				inStream.close();
				throw Res2hException(std::string("loadArchive() - Failed to find archive in file \"") + archivePath + "\"!");
			}
		}
		//magic bytes ok. get size of whole file.
		inStream.seekg(archiveOffset + RES2H_OFFSET_ARCHIVE_SIZE);
		uint32_t archiveSize = 0;
		inStream.read(reinterpret_cast<char *>(&archiveSize), sizeof(uint32_t));
		//control checksum. close first for calling checksum function
		inStream.close();
		uint32_t calculatedChecksum = calculateAdler32(archivePath, archiveSize - sizeof(uint32_t));
		//re-open again
		inStream.open(archivePath, std::ifstream::in | std::ifstream::binary);
		//read checksum from end of file
		inStream.seekg(archiveOffset + archiveSize - sizeof(uint32_t));
		uint32_t fileChecksum = 0;
		inStream.read(reinterpret_cast<char *>(&fileChecksum), sizeof(uint32_t));
		if (fileChecksum == calculatedChecksum) {
			//nice. checksum is ok. rewind and skip behind magic bytes
			inStream.seekg(archiveOffset + RES2H_OFFSET_FILE_VERSION);
			//check file version
			uint32_t fileVersion = 0;
			inStream.read(reinterpret_cast<char *>(&fileVersion), sizeof(uint32_t));
			if (fileVersion <= RES2H_ARCHIVE_VERSION) {
				//file version ok. skip currently unused flags and archive size
				inStream.seekg(archiveOffset + RES2H_OFFSET_NO_OF_FILES);
				//read number of directory entries
				uint32_t nrOfDirectoryEntries = 0;
				inStream.read(reinterpret_cast<char *>(&nrOfDirectoryEntries), sizeof(uint32_t));
				if (nrOfDirectoryEntries > 0) {
					for (uint32_t i = 0; i < nrOfDirectoryEntries; ++i) {
						//read directory to map
						ResourceEntry temp;
						//read size of name
						uint32_t sizeOfName = 0;
						inStream.read(reinterpret_cast<char *>(&sizeOfName), sizeof(uint32_t));
						//read name itself
						temp.filePath.resize(sizeOfName);
						inStream.read(reinterpret_cast<char *>(&temp.filePath[0]), sizeOfName - 1);
						//skip the null-terminator
						inStream.seekg(1, std::ios::cur);
						//clear data pointer
						temp.data = nullptr;
						//skip currently unused flags
						inStream.seekg(sizeof(uint32_t), std::ios::cur);
						//read size of data
						inStream.read(reinterpret_cast<char *>(&temp.dataSize), sizeof(uint32_t));
						//read offset to start of data
						inStream.read(reinterpret_cast<char *>(&temp.dataOffset), sizeof(uint32_t));
						//read data checksum
						inStream.read(reinterpret_cast<char *>(&temp.checksum), sizeof(uint32_t));
						//add archive path and offset
						temp.archivePath = archivePath;
						temp.archiveStart = archiveOffset;
						//add to map
						resourceMap[temp.filePath] = temp;
					}
					//close file
					inStream.close();
					return true;
				}
				else {
					inStream.close();
					throw Res2hException(std::string("loadArchive() - Archive \"") + archivePath + "\" contains no files!");
				}
			}
			else {
				inStream.close();
				throw Res2hException(std::string("loadArchive() - Archive \"") + archivePath + "\" has a newer file version and can not be read!");
			}
		}
		else {
			inStream.close();
			throw Res2hException(std::string("loadArchive() - Archive \"") + archivePath + "\" has a bad checksum!");
		}
	}
	else {
        throw Res2hException(std::string("loadArchive() - Failed to open archive \"") + archivePath + "\" for reading.");
    }
	return false;
}

Res2h::ResourceEntry Res2h::loadFileFromDisk(const std::string & filePath)
{
    ResourceEntry temp;
    //try to open file
    std::ifstream inStream;
    inStream.open(filePath, std::ifstream::in | std::ifstream::binary);
    if (inStream.is_open() && inStream.good()) {
        //opened ok. check file size
        inStream.seekg(0, std::ios::end);
        size_t fileSize = (size_t)inStream.tellg();
        inStream.seekg(0);
        if (fileSize > 0) {
            //allocate data
            std::shared_ptr<unsigned char> fileData = std::shared_ptr<unsigned char>(new unsigned char[fileSize], [](unsigned char *p) { delete[] p; });
            //try reading data
            try {
                inStream.read(reinterpret_cast<char *>(fileData.get()), fileSize);
            }
            catch (std::ios_base::failure) { /*reading didn't work properly. salvage what we can.*/ }
            //check how many bytes were actually read
            if (inStream.gcount() != fileSize) {
                throw Res2hException(std::string("loadFileFromDisk() - Failed to read file \"") + filePath + "\".");
            }
            //seems to have worked. store data.
            temp.filePath = filePath;
            temp.data = fileData;
            temp.dataSize = fileSize;
        }
    }
    else {
        throw Res2hException(std::string("loadFileFromDisk() - Failed to open file \"") + filePath + "\" for reading.");
    }
    return temp;
}

Res2h::ResourceEntry Res2h::loadFileFromArchive(const Res2h::ResourceEntry & entry)
{
    ResourceEntry temp = entry;
    //try to open archive file
    std::ifstream inStream;
    inStream.open(temp.archivePath, std::ifstream::in | std::ifstream::binary);
    if (inStream.is_open() && inStream.good()) {
        //opened ok. move to data offset
        inStream.seekg(temp.archiveStart + temp.dataOffset);
        //allocate data
        temp.data = std::shared_ptr<unsigned char>(new unsigned char[temp.dataSize], [](unsigned char *p) { delete[] p; });
        //try reading data
        try {
            inStream.read(reinterpret_cast<char *>(temp.data.get()), temp.dataSize);
        }
        catch (std::ios_base::failure) { /*reading didn't work properly. salvage what we can.*/ }
        //check how many bytes were actually read
        if (inStream.gcount() != temp.dataSize) {
            throw Res2hException(std::string("loadFileFromArchive() - Failed to read \"") + temp.filePath + "\" from archive.");
        }
    }
    else {
        throw Res2hException(std::string("loadFileFromArchive() - Failed to open archive \"") + temp.archivePath + "\" for reading.");
    }
    return temp;
}

Res2h::ResourceEntry Res2h::loadFile(const std::string & filePath, bool keepInCache, bool checkChecksum)
{
	ResourceEntry temp;
    //find file in the map
    for (auto rmIt = resourceMap.begin(); rmIt != resourceMap.end(); ++rmIt) {
        if (rmIt->second.filePath == filePath) {
			//found. check if still in memory.
			temp = rmIt->second;
			if (temp.data) {
				//data is still valid. return it.
				return temp;
			}
            //data needs to be loaded. check if archive file
            if (filePath.find_first_of(":/") == 0) {
                //yes. try to load archive
                temp = loadFileFromArchive(temp);
                //calculate checksum if user wants to
                if (checkChecksum) {
                    if (temp.checksum != calculateAdler32(temp.data.get(), temp.dataSize)) {
                        throw Res2hException(std::string("loadFile() - Bad checksum for \"") + filePath + "\".");
                    }
                }
            }
            else {
				//try to open disk file
				temp = loadFileFromDisk(temp.filePath);
            }
            //loaded. if the user wants to cache the data, add it to our map, else just return it and it will be freed eventually
            if (keepInCache) {
                rmIt->second.data = temp.data;
            }
            return temp;
        }
    }
	//when we get here the file was not in the map. try to load it
	if (filePath.find_first_of(":/") == 0) {
		//archive file. we can't load files from archives we have no directory for. notify caller.
		throw Res2hException(std::string("loadFile() - File \"") + filePath + "\" is unkown. Please use loadArchive() first.");
	}
	else {
		//try to open disk file
		temp = loadFileFromDisk(temp.filePath);
		//loaded. if the user wants to cache the data, add it to our map, else just return it and it will be freed eventually
		if (keepInCache) {
            resourceMap[temp.filePath] = temp;
		}
	}
	return temp;
}

size_t Res2h::getNrOfResources()
{
	return resourceMap.size();
}

Res2h::ResourceEntry Res2h::getResource(size_t index)
{
	if (index < resourceMap.size()) {
		auto rmIt = resourceMap.cbegin();
		advance(rmIt, index);
		if (rmIt != resourceMap.end()) {
			return rmIt->second;
		}
		else {
			throw Res2hException(std::string("getFile() - Index ") + std::to_string((long long)index) + " out of bounds! Map has " + std::to_string((long long)resourceMap.size()) + "files.");
		}
	}
	else {
		throw Res2hException(std::string("getFile() - Index ") + std::to_string((long long)index) + " out of bounds! Map has " + std::to_string((long long)resourceMap.size()) + "files.");
	}
}

void Res2h::releaseCache()
{
    //erase all allocated data in resource map
    auto rmIt = resourceMap.begin();
    while (rmIt != resourceMap.end()) {
        if (rmIt->second.data) {
            //shared_ptr is valid, so there is allocated data. release our reference to it.
            rmIt->second.data.reset();
        }
        ++rmIt;
    }
}

uint32_t Res2h::calculateChecksum(const std::string & filePath, const size_t dataSize, uint32_t adler)
{
	return calculateAdler32(filePath, dataSize, adler);
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
