#include "turnout/TurnoutMqttCommandDecoder.h"
#include <algorithm>

bool TurnoutMqttCommandDecoder::canDecode(const std::string& deviceTypeSegment) const
{
    return deviceTypeSegment == "turnout";
}

std::optional<int> TurnoutMqttCommandDecoder::parseAddress(const std::string& address)
{
    if (address.empty() || address.size() > 4)
    {
        return std::nullopt;
    }
    if (!std::all_of(address.begin(), address.end(), [](char c) { return c >= '0' && c <= '9'; }))
    {
        return std::nullopt;
    }
    const int value = std::stoi(address);
    if (value < 1 || value > 2048)
    {
        return std::nullopt;
    }
    return value;
}

std::optional<TurnoutPosition> TurnoutMqttCommandDecoder::parsePosition(const std::string& payload)
{
    if (payload == "CLOSED")
    {
        return TurnoutPosition::Closed;
    }
    if (payload == "THROWN")
    {
        return TurnoutPosition::Thrown;
    }
    return std::nullopt;
}

std::optional<DomainCommand> TurnoutMqttCommandDecoder::decode(const std::string& address,
                                                                 const std::string& payload) const
{
    auto parsedAddress = parseAddress(address);
    auto parsedPosition = parsePosition(payload);
    if (!parsedAddress.has_value() || !parsedPosition.has_value())
    {
        return std::nullopt;
    }
    return DomainCommand(SetTurnoutPosition(TurnoutAddress(*parsedAddress), *parsedPosition));
}
