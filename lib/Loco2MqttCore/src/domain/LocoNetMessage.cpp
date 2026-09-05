#include "LocoNetMessage.h"

#include <iomanip>
#include <sstream>

namespace
{
    std::string byteToHex(uint8_t value)
    {
        std::ostringstream out;
        out << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << static_cast<int>(value);
        return out.str();
    }
}

std::string LocoNetMessage::describe() const
{
    std::string result;
    for (size_t i = 0; i < bytes_.size(); ++i)
    {
        result += (i > 0 ? " " : "") + byteToHex(bytes_[i]);
    }
    return result;
}
