#pragma once

#include <array>
#include <fstream>
#include <cstdint>
#include <string>

/// @brief Create Fletcher checksum from data.
/// @param[in] data Data to create checksum for.
/// @param[in] dataSize The size of the data to incorporate in the checksum.
/// @param[in] checksum Optional. Fletcher checksum from last run if you're using more than one file.
/// @return Returns the Fletcher checksum for the file stream or the initial checksum upon failure.
/// @note Based on this: https://en.wikipedia.org/wiki/Fletcher's_checksum.
template <typename T>
T calculateFletcher(const uint8_t *data, T dataSize, T checksum = 0);

/// @brief Create Fletcher checksum from file. Builds checksum from start position till EOF.
/// @param[in] filePath Path to the file to build the checksum for.
/// @param[in] dataSize Optional. The size of the data to incorporate in the checksum. Pass 0 to scan whole file.
/// @param[in] checksum Optional. Adler checksum from last run if you're using more than one file.
/// @return Returns the Fletcher checksum for the file stream or the initial checksum upon failure.
/// @note Based on this: https://en.wikipedia.org/wiki/Fletcher's_checksum.
template <typename T>
T calculateFletcher(const std::string &filePath, const T dataSize = 0, T checksum = 0)
{
    // open file
    std::ifstream inStream;
    inStream.open(filePath, std::ios_base::in | std::ios_base::binary);
    if (inStream.is_open() && inStream.good())
    {
        // loop until EOF or dataSize reached
        T rollingSize = 0;
		std::array<uint8_t, 4096> buffer{};
        while (!inStream.eof() && inStream.good())
        {
            T readSize = sizeof(buffer);
            try
            {
                // try reading data from input file
                inStream.read(reinterpret_cast<char *>(&buffer), sizeof(buffer));
            }
            catch (const std::ios_base::failure &/*e*/)
            {
				// check if were at EOF, or if reading failed for some other reason
				if (!inStream.eof())
				{
					throw std::runtime_error("calculateFletcher - File read error");
				}
            }
            // store how many bytes were actually read
            readSize = static_cast<T>(inStream.gcount());
            // clamp to dataSize if the caller wants to checksum only part of the file
            if (dataSize > 0 && rollingSize + readSize > dataSize)
            {
                readSize = dataSize - rollingSize;
            }
            // calculate checksum for buffer
            checksum = calculateFletcher<T>(buffer.data(), readSize, checksum);
            // update size already read
            rollingSize += readSize;
            if (dataSize > 0 && rollingSize >= dataSize)
            {
                break;
            }
        }
        // close file
        inStream.close();
    }
	else
	{
		throw std::runtime_error("calculateFletcher - Failed to open file for reading");
	}
	
    return checksum;
}
