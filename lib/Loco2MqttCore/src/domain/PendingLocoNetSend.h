#pragma once

#include <utility>

#include "domain/LocoNetMessage.h"

class PendingLocoNetSend
{
public:
    PendingLocoNetSend(LocoNetMessage message, unsigned long dueAtMilliseconds)
        : message_(std::move(message)), dueAtMilliseconds_(dueAtMilliseconds)
    {
    }

    const LocoNetMessage& message() const
    {
        return message_;
    }

    unsigned long dueAtMilliseconds() const
    {
        return dueAtMilliseconds_;
    }

private:
    LocoNetMessage message_;
    unsigned long dueAtMilliseconds_;
};
