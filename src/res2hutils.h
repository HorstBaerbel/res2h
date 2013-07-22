#pragma once

#include <stdint.h>
#include <string>

/*
Create Adler-32 checksum from file. Builds checksum from start position till EOF.
\param[in] filePath Path to the file to build the checksum for.
\param[in] adler Optional. Adler checksum from last run if you're using more than one file.
\return Returns the Adler-32 checksum for the file stream or the initial checksum upon failure.
\note Based on the sample code here: https://tools.ietf.org/html/rfc1950. This is not as safe as CRC-32 (see here: https://en.wikipedia.org/wiki/Adler-32), but should be totally sufficient for us.
*/
uint32_t calculateAdler32(const std::string & filePath, uint32_t adler = 1);
