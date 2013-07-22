#include "res2hinterface.hpp"

#include <fstream>


std::map<std::string, Res2h::ResourceEntry> Res2h::resourceMap;


bool Res2h::loadArchive(const std::string & archivePath)
{
    //check if there are entries for this archive already in the map and delete them
    std::map<std::string, ResourceEntry>::iterator rmIt = resourceMap.begin();
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
        //worked. read magic bytes
        unsigned char magicBytes[8] = {0};
        inStream.read(reinterpret_cast<char *>(&magicBytes), sizeof(magicBytes));
        std::string magicString(reinterpret_cast<char *>(&magicBytes), sizeof(magicBytes));
        if (magicString == "res2hbin") {
            //magic bytes ok. control checksum. close first for calling checksum function
            inStream.close();
            uint32_t calculatedChecksum = calculateAdler32(archivePath);
            //re-open again
            inStream.open(archivePath, std::ifstream::in | std::ifstream::binary);
            //read checksum from end of file
            inStream.seekg(sizeof(uint32_t), std::ios::end);
            uint32_t fileChecksum = 0;
            inStream.read(reinterpret_cast<char *>(&fileChecksum), sizeof(uint32_t));
            if (fileChecksum == calculatedChecksum) {
                //nice. checksum is ok. rewind and skip behind magic bytes
                inStream.seekg(sizeof(magicBytes), std::ios::beg);
                //check file version
                uint32_t fileVersion = 0;
                inStream.read(reinterpret_cast<char *>(&fileVersion), sizeof(uint32_t));
                if (fileVersion <= ARCHIVE_VERSION) {
                    //file version ok. skip currently unused flags
                    inStream.seekg(sizeof(uint32_t), std::ios::cur);
                    //read number of directory entries
                    uint32_t nrOfDirectoryEntries = 0;
                    inStream.read(reinterpret_cast<char *>(&fileVersion), sizeof(uint32_t));
                    if (nrOfDirectoryEntries > 0) {
                        for (uint32_t i = 0; i < nrOfDirectoryEntries; ++i) {
                            //read directory to map
                            ResourceEntry temp;
                            //read size of name
                            uint32_t sizeOfName = 0;
                            inStream.read(reinterpret_cast<char *>(&sizeOfName), sizeof(uint32_t));
                            //read name itself
                            temp.fileName.resize(sizeOfName);
                            inStream.read(reinterpret_cast<char *>(&temp.fileName[0]), sizeOfName - 1);
                            //clear data pointer
                            temp.data = nullptr;
                            //skip currently unused flags
                            inStream.seekg(sizeof(uint32_t), std::ios::cur);
                            //read size of data
                            inStream.read(reinterpret_cast<char *>(&temp.size), sizeof(uint32_t));
                            //read offset to start of data
                            inStream.read(reinterpret_cast<char *>(&temp.dataOffset), sizeof(uint32_t));
                            //read data checksum
                            inStream.read(reinterpret_cast<char *>(&temp.checksum), sizeof(uint32_t));
                            //add archive path
                            temp.archivePath = archivePath;
                            //add to map
                            resourceMap[temp.fileName] = temp;
                        }
                        //close file
                        inStream.close();
                        return true;
                    }
                    else {
                        throw Res2hException(std::string("Archive \"") + archivePath + "\" contains no files!");
                    }
                }
                else {
                    throw Res2hException(std::string("Archive \"") + archivePath + "\" has a newer file version and can not be read!");
                }
            }
            else {
                throw Res2hException(std::string("Archive \"") + archivePath + "\" has a bad checksum!");
            }
        }
        else {
            throw Res2hException(std::string("File \"") + archivePath + "\" doesn't seem to be a res2h archive!");
        }
    }
    else {
        throw Res2hException(std::string("Failed to open archive \"") + archivePath + "\".");
    }
	return false;
}

Res2h::ResourceEntry Res2h::loadFile(const std::string & filePath, bool keepInCache)
{
	ResourceEntry temp;
	return temp;
}

void Res2h::releaseCache()
{
    //erase all allocated data in resource map
    std::map<std::string, ResourceEntry>::iterator rmIt = resourceMap.begin();
    while (rmIt != resourceMap.end()) {
        if (rmIt->second.data) {
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
