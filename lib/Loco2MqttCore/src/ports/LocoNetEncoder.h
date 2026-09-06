#pragma once

#include "domain/DomainCommand.h"
#include "ports/LocoNetSendScheduler.h"

class LocoNetEncoder
{
public:
    virtual ~LocoNetEncoder() = default;
    virtual void encode(const DomainCommand& command, LocoNetSendScheduler& scheduler) const = 0;
};
