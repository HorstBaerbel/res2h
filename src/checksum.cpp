#include "checksum.h"

template <>
uint16_t calculateFletcher(const uint8_t *data, uint16_t dataSize, uint16_t checksum)
{
    auto sum1 = static_cast<uint8_t>(checksum);
    auto sum2 = static_cast<uint8_t>(checksum >> 8);
    if (data != nullptr && dataSize > 0)
    {
        for (uint16_t index = 0; index < dataSize; ++index)
        {
            // casts are neded here, because of implicit converstion to int
            sum1 = static_cast<decltype(sum1)>(sum1 + data[index]);
            sum2 = static_cast<decltype(sum1)>(sum2 + sum1);
        }
    }
    // casts are neded here, because of implicit converstion to int
    return static_cast<uint16_t>((static_cast<uint16_t>(sum2) << 8) | sum1);
}

template <>
uint32_t calculateFletcher(const uint8_t *data, uint32_t dataSize, uint32_t checksum)
{
    // calculate how many full words the input has
    const uint32_t words = dataSize / 2;
    auto sum1 = static_cast<uint16_t>(checksum);
    auto sum2 = static_cast<uint16_t>(checksum >> 16);
    if (data != nullptr && dataSize > 0)
    {
        // now calculate the fletcher32 checksum from words
        auto *data16 = reinterpret_cast<const uint16_t *>(data);
        for (uint32_t index = 0; index < words; ++index)
        {
            // casts are neded here, because of implicit converstion to int
            sum1 = static_cast<decltype(sum1)>(sum1 + data16[index]);
            sum2 = static_cast<decltype(sum1)>(sum2 + sum1);
        }
        // calculate how many extra bytes, that do not fit into a word, the input has
        const uint32_t remainingBytes = dataSize - words * 2;
        if (remainingBytes > 0)
        {
            // copy the excess byte to our dummy variable
            uint16_t dummy = 0;
            reinterpret_cast<uint8_t *>(&dummy)[0] = data[words * 2];
            // now add the dummy on top (casts are neded here, because of implicit converstion to int)
            sum1 = static_cast<decltype(sum1)>(sum1 + dummy);
            sum2 = static_cast<decltype(sum1)>(sum2 + sum1);
        }
    }
    return (static_cast<uint32_t>(sum2) << 16) | sum1;
}

template <>
uint64_t calculateFletcher(const uint8_t *data, uint64_t dataSize, uint64_t checksum)
{
    // calculate how many full dwords the input has
    const uint64_t dwords = dataSize / 4;
    auto sum1 = static_cast<uint32_t>(checksum);
    auto sum2 = static_cast<uint32_t>(checksum >> 32);
    if (data != nullptr && dataSize > 0)
    {
        // now calculate the fletcher64 checksum from dwords
        auto data32 = reinterpret_cast<const uint32_t *>(data);
        for (uint64_t index = 0; index < dwords; ++index)
        {
            sum1 += data32[index];
            sum2 += sum1;
        }
        // calculate how many extra bytes, that do not fit into a dword, the input has
        const uint64_t remainingBytes = dataSize - dwords * 4;
        if (remainingBytes > 0)
        {
            // copy the excess bytes to our dummy variable
            uint32_t dummy = 0;
            for (uint64_t index = 0; index < remainingBytes; ++index)
            {
                reinterpret_cast<uint8_t *>(&dummy)[index] = data[dwords * 4 + index];
            }
            // now add the dummy on top
            sum1 += dummy;
            sum2 += sum1;
        }
    }
    return (static_cast<uint64_t>(sum2) << 32) | sum1;
}
