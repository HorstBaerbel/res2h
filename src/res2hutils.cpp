#include "res2hutils.hpp"

#include <fstream>


uint32_t calculateAdler32(const unsigned char * data, const size_t dataSize, uint32_t adler)
{
	uint32_t s1 = adler & 0xffff;
	uint32_t s2 = (adler >> 16) & 0xffff;
	//calculate checksum for buffer
	for (std::streamsize n = 0; n < dataSize; n++) {
		s1 = (s1 + data[n]) % 65521;
		s2 = (s2 + s1) % 65521;
	}
	//build final checksum
	return (s2 << 16) + s1;
}

uint32_t calculateAdler32(const std::string & filePath, const size_t dataSize, uint32_t adler)
{
	//open file
	std::ifstream inStream;
	inStream.open(filePath, std::ifstream::in | std::ifstream::binary);
	if (inStream.is_open() && inStream.good()) {
		//loop until EOF or dataSize reached
		std::streamsize rollingSize = 0;
		while (!inStream.eof() && inStream.good()) {
			unsigned char buffer[1024];
			std::streamsize readSize = sizeof(buffer);
			try {
				//try reading data from input file
				inStream.read(reinterpret_cast<char *>(&buffer), sizeof(buffer));
			}
			catch (std::ios_base::failure) { /*reading didn't work properly. salvage what we can.*/ }
			//store how many bytes were actually read
			readSize = inStream.gcount();
			//clamp to dataSize if the caller wants to checksum only part of the file
			if (dataSize > 0 && rollingSize + readSize > dataSize) {
				readSize = dataSize - rollingSize;
			}
			//calculate checksum for buffer
			adler = calculateAdler32((const unsigned char*)&buffer, (size_t)readSize, adler);
			//update size already read
			rollingSize += readSize;
			if (dataSize > 0 && rollingSize >= dataSize) {
				break;
			}
		}
		//close file
		inStream.close();
	}
	return adler;
}
