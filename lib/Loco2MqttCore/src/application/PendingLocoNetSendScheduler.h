#pragma once

#include <vector>

#include "domain/PendingLocoNetSend.h"
#include "ports/Clock.h"
#include "ports/LocoNetPort.h"
#include "ports/LocoNetSendScheduler.h"

class PendingLocoNetSendScheduler : public LocoNetSendScheduler
{
public:
    PendingLocoNetSendScheduler(LocoNetPort& port, Clock& clock) : port_(port), clock_(clock)
    {
    }

    void sendNow(const LocoNetMessage& message) override;
    void sendAfter(const LocoNetMessage& message, unsigned long delayMilliseconds) override;
    void update();

private:
    LocoNetPort& port_;
    Clock& clock_;
    std::vector<PendingLocoNetSend> pending_;
};
