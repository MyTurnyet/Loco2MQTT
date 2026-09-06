#pragma once

#include "domain/LocoNetMessage.h"

class LocoNetSendScheduler
{
public:
    virtual ~LocoNetSendScheduler() = default;
    virtual void sendNow(const LocoNetMessage& message) = 0;
    virtual void sendAfter(const LocoNetMessage& message, unsigned long delayMilliseconds) = 0;
};
