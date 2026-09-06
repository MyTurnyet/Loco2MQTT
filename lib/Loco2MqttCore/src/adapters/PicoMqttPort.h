#pragma once

#ifdef ARDUINO

#include <PicoMQTT.h>

#include <deque>
#include <string>

#include "ports/MqttPort.h"

class PicoMqttPort : public MqttPort
{
public:
    PicoMqttPort();

    void begin();
    void update();
    void publish(const MqttMessage& message) override;
    std::optional<IncomingMqttMessage> receiveCommand() override;

private:
    PicoMQTT::Server server_;
    std::deque<IncomingMqttMessage> pendingCommands_;

    // Bounds worst-case heap growth from a client publishing commands faster
    // than MqttCommandRouter::update() drains them. Unlike the LocoNet
    // receive queue's small number of trusted local devices, this queue is
    // fed by anyone who can reach the broker over the network. Oldest
    // entries are dropped first, matching LocoNetEsp32Port's
    // kMaxPendingMessages precedent.
    static constexpr std::size_t kMaxPendingCommands = 32;
};

#endif
