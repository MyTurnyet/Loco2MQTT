#pragma once

#include "ports/LocoNetEncoder.h"

class TurnoutLocoNetEncoder : public LocoNetEncoder
{
public:
    void encode(const DomainCommand& command, LocoNetSendScheduler& scheduler) const override;

private:
    static constexpr unsigned long kOffPulseDelayMs = 250;
};
