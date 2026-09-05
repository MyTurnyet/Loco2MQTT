#pragma once

#include <optional>

#include "domain/LocoNetMessage.h"

class LocoNetPort
{
public:
    virtual ~LocoNetPort() = default;
    virtual std::optional<LocoNetMessage> receive() = 0;
    virtual void send(const LocoNetMessage& message) = 0;
};
