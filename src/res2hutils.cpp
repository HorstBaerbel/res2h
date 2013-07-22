#include "res2hutils.h"

#include <fstream>

/*
Create Adler-32 checksum from file. Builds checksum from start position till EOF.
\param[in] filePath Path to the file to build the checksum for.
\param[in] adler Optional. Adler checksum from last run if you're using more than one file.
\return Returns the Adler-32 checksum for the file stream or the initial checksum upon failure.
\note Based on the sample code here: https://tools.ietf.org/html/rfc1950. This is not as safe as CRC-32 (see here: https://en.wikipedia.org/wiki/Adler-32), but should be totally sufficient for us.
*/
uint32_t calculateAdler32(const std::string & filePath, uint32_t adler)
{
	//open file
	std::ifstream inStream;
	inStream.open(filePath, std::ifstream::in | std::ifstream::binary);
	if (inStream.is_open() && inStream.good()) {
		//build checksum
		uint32_t s1 = adler & 0xffff;
		uint32_t s2 = (adler >> 16) & 0xffff;
		//loop until EOF
		while (!inStream.eof() && inStream.good()) {
			char buffer[1024];
			std::streamsize readSize = sizeof(buffer);
			try {
				//try reading data from input file
				inStream.read(reinterpret_cast<char *>(&buffer), sizeof(buffer));
			}
			catch (std::ios_base::failure) {
				//reading didn't work properly. store how many bytes were actually read
				readSize = inStream.gcount();
			}
			//calculate checksum for buffer
			for (std::streamsize n = 0; n < readSize; n++) {
				s1 = (s1 + buffer[n]) % 65521;
				s2 = (s2 + s1) % 65521;
			}
		}
		//close file
		inStream.close();
		//build final checksum
		return (s2 << 16) + s1;
	}
	return adler;
}
