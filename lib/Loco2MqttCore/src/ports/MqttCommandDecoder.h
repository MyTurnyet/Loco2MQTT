#pragma once

#include <optional>
#include <string>

#include "domain/DomainCommand.h"

class MqttCommandDecoder
{
public:
    virtual ~MqttCommandDecoder() = default;
    virtual bool canDecode(const std::string& deviceTypeSegment) const = 0;
    virtual std::optional<DomainCommand> decode(const std::string& address, const std::string& payload) const = 0;
};
