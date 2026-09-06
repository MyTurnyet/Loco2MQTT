#pragma once

#include <utility>
#include <vector>

#include "ports/Clock.h"
#include "ports/LocoNetMessageDecoder.h"
#include "ports/LocoNetPort.h"
#include "ports/MessageLog.h"
#include "ports/MqttEventEncoder.h"
#include "ports/MqttPort.h"

class LocoNetMessageRouter
{
public:
    // Owns the sole drain of LocoNetPort::receive() in normal operation, so
    // it also records every message to MessageLog itself -- a separate
    // LocoNetMessageLogger draining the same port would race this router
    // for a destructive-read queue and starve it (found in final review).
    LocoNetMessageRouter(LocoNetPort& locoNetPort, MqttPort& mqttPort, Clock& clock, MessageLog& log,
                          std::vector<std::pair<LocoNetMessageDecoder*, MqttEventEncoder*>> decoders)
        : locoNetPort_(locoNetPort), mqttPort_(mqttPort), clock_(clock), log_(log), decoders_(std::move(decoders))
    {
    }

    void update();

private:
    void handleMessage(const LocoNetMessage& message);
    void republishAllKnownStatesIfDue();

    LocoNetPort& locoNetPort_;
    MqttPort& mqttPort_;
    Clock& clock_;
    MessageLog& log_;
    std::vector<std::pair<LocoNetMessageDecoder*, MqttEventEncoder*>> decoders_;
    unsigned long lastRepublishAtMilliseconds_ = 0;

    static constexpr unsigned long kStateRepublishIntervalMs = 30000;
};
