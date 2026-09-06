#pragma once

#include "domain/SetTurnoutPosition.h"
#include "ports/MqttCommandDecoder.h"

class TurnoutMqttCommandDecoder : public MqttCommandDecoder
{
public:
    bool canDecode(const std::string& deviceTypeSegment) const override;
    std::optional<DomainCommand> decode(const std::string& address, const std::string& payload) const override;
};
