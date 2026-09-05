#pragma once

#include "domain/LocoNetMessage.h"

class MessageLog
{
public:
    virtual ~MessageLog() = default;
    virtual void record(const LocoNetMessage& message) = 0;
};
