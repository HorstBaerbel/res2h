#pragma once

#include <stdint.h>
#include <string>

/*
Create Adler-32 checksum from file. Builds checksum from start position till EOF.
\param[in] filePath Path to the file to build the checksum for.
\param[in] dataSize Optional. The size of the data to incorporate in the checksum. Pass 0 to scan whole file.
\param[in] adler Optional. Adler checksum from last run if you're using more than one file.
\return Returns the Adler-32 checksum for the file stream or the initial checksum upon failure.
\note Based on the sample code here: https://tools.ietf.org/html/rfc1950. This is not as safe as CRC-32 (see here: https://en.wikipedia.org/wiki/Adler-32), but should be totally sufficient for us.
*/
uint32_t calculateAdler32(const std::string & filePath, const size_t dataSize = 0, uint32_t adler = 1);

/*
Create Adler-32 checksum from data.
\param[in] data Data to create checksum for.
\param[in] dataSize The size of the data to incorporate in the checksum.
\param[in] adler Optional. Adler checksum from last run if you're using more than one file.
\return Returns the Adler-32 checksum for the file stream or the initial checksum upon failure.
\note Based on the sample code here: https://tools.ietf.org/html/rfc1950. This is not as safe as CRC-32 (see here: https://en.wikipedia.org/wiki/Adler-32), but should be totally sufficient for us.
*/
uint32_t calculateAdler32(const unsigned char * data, const size_t dataSize, uint32_t adler = 1);
