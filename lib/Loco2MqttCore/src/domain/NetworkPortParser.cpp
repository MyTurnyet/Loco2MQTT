#include "NetworkPortParser.h"

namespace
{
    bool isAllDigits(const std::string& text)
    {
        return !text.empty() && text.find_first_not_of("0123456789") == std::string::npos;
    }
}

std::optional<uint16_t> parseNetworkPort(const std::string& text)
{
    if (!isAllDigits(text))
    {
        return std::nullopt;
    }
    const unsigned long value = std::stoul(text);
    if (value < 1 || value > 65535)
    {
        return std::nullopt;
    }
    return static_cast<uint16_t>(value);
}
