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

    // Grows by one per accepted MQTT `set` command until its off-pulse
    // fires ~250ms later. Bounded by offered-command-rate x the 250ms
    // off-pulse delay, not by PicoMqttPort's 32-entry cap alone -- that cap
    // only limits how many commands accumulate between two
    // MqttCommandRouter::update() drains, not the total in flight here.
    std::vector<PendingLocoNetSend> pending_;
};
