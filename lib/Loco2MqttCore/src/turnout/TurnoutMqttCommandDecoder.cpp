#include "turnout/TurnoutMqttCommandDecoder.h"
#include <algorithm>

bool TurnoutMqttCommandDecoder::canDecode(const std::string& deviceTypeSegment) const
{
    return deviceTypeSegment == "turnout";
}

bool TurnoutMqttCommandDecoder::isValidAddressShape(const std::string& address)
{
    return !address.empty() && address.size() <= 4 &&
           std::all_of(address.begin(), address.end(), [](char c) { return c >= '0' && c <= '9'; });
}

std::optional<int> TurnoutMqttCommandDecoder::parseAddress(const std::string& address)
{
    if (!isValidAddressShape(address))
    {
        return std::nullopt;
    }
    const int value = std::stoi(address);
    return (value >= 1 && value <= 2048) ? std::optional<int>(value) : std::nullopt;
}

std::optional<TurnoutPosition> TurnoutMqttCommandDecoder::parsePosition(const std::string& payload)
{
    if (payload == "CLOSED")
    {
        return TurnoutPosition::Closed;
    }
    return payload == "THROWN" ? std::optional<TurnoutPosition>(TurnoutPosition::Thrown) : std::nullopt;
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
