#pragma once

#include <cstdint>
#include <vector>

inline uint8_t computeLocoNetChecksum(const std::vector<uint8_t>& bytesBeforeChecksum)
{
    uint8_t xorAccumulator = 0;
    for (uint8_t byte : bytesBeforeChecksum)
    {
        xorAccumulator ^= byte;
    }
    return xorAccumulator ^ 0xFF;
}
