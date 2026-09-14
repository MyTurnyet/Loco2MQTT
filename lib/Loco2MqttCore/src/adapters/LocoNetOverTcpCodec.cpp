#include "LocoNetOverTcpCodec.h"

#include <cctype>
#include <sstream>
#include <vector>

#include "domain/LocoNetChecksum.h"

namespace
{
    const std::string kReceivePrefix = "RECEIVE ";

    bool isHexByteToken(const std::string& token)
    {
        return token.size() == 2 && std::isxdigit(static_cast<unsigned char>(token[0])) &&
               std::isxdigit(static_cast<unsigned char>(token[1]));
    }

    std::optional<std::vector<uint8_t>> parseHexBytes(const std::string& body)
    {
        std::vector<uint8_t> bytes;
        std::istringstream stream(body);
        std::string token;
        while (stream >> token)
        {
            if (!isHexByteToken(token))
            {
                return std::nullopt;
            }
            bytes.push_back(static_cast<uint8_t>(std::stoi(token, nullptr, 16)));
        }
        return bytes.empty() ? std::nullopt : std::make_optional(bytes);
    }

    // The real-hardware LocoNetPort trusts the vendor library to have
    // already validated checksums (see CLAUDE.md); this interim transport
    // has no equivalent upstream guarantee, so it checks its own.
    bool hasValidChecksum(const std::vector<uint8_t>& bytes)
    {
        if (bytes.size() < 2)
        {
            return false;
        }
        std::vector<uint8_t> bytesBeforeChecksum(bytes.begin(), bytes.end() - 1);
        return computeLocoNetChecksum(bytesBeforeChecksum) == bytes.back();
    }
}

std::string LocoNetOverTcpCodec::encodeSend(const LocoNetMessage& message) const
{
    return "SEND " + message.describe();
}

std::optional<LocoNetMessage> LocoNetOverTcpCodec::decodeLine(const std::string& line) const
{
    if (line.rfind(kReceivePrefix, 0) != 0)
    {
        return std::nullopt;
    }
    auto bytes = parseHexBytes(line.substr(kReceivePrefix.size()));
    if (!bytes.has_value() || !hasValidChecksum(*bytes))
    {
        return std::nullopt;
    }
    return LocoNetMessage(*bytes);
}
