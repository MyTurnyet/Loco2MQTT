#pragma once

#include <cstdint>
#include <string>
#include <vector>

class LocoNetMessage
{
public:
    explicit LocoNetMessage(std::vector<uint8_t> bytes) : bytes_(std::move(bytes))
    {
    }

    const std::vector<uint8_t>& bytes() const
    {
        return bytes_;
    }

    std::string describe() const;

    bool operator==(const LocoNetMessage& other) const
    {
        return bytes_ == other.bytes_;
    }

    bool operator!=(const LocoNetMessage& other) const
    {
        return !(*this == other);
    }

private:
    std::vector<uint8_t> bytes_;
};
