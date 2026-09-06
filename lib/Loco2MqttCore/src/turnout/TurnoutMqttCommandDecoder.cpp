#include "turnout/TurnoutMqttCommandDecoder.h"

namespace
{
    bool isValidAddressString(const std::string& address)
    {
        if (address.empty() || address.size() > 4)
        {
            return false;
        }
        for (char c : address)
        {
            if (c < '0' || c > '9')
            {
                return false;
            }
        }
        return true;
    }
}

bool TurnoutMqttCommandDecoder::canDecode(const std::string& deviceTypeSegment) const
{
    return deviceTypeSegment == "turnout";
}

std::optional<DomainCommand> TurnoutMqttCommandDecoder::decode(const std::string& address,
                                                                 const std::string& payload) const
{
    if (!isValidAddressString(address))
    {
        return std::nullopt;
    }
    const int value = std::stoi(address);
    if (value < 1 || value > 2048)
    {
        return std::nullopt;
    }
    if (payload != "CLOSED" && payload != "THROWN")
    {
        return std::nullopt;
    }
    const TurnoutPosition position = payload == "CLOSED" ? TurnoutPosition::Closed : TurnoutPosition::Thrown;
    return DomainCommand(SetTurnoutPosition(TurnoutAddress(value), position));
}
