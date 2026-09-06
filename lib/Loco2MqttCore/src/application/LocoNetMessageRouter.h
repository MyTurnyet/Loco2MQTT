#pragma once

#include <utility>
#include <vector>

#include "ports/Clock.h"
#include "ports/LocoNetMessageDecoder.h"
#include "ports/LocoNetPort.h"
#include "ports/MqttEventEncoder.h"
#include "ports/MqttPort.h"

class LocoNetMessageRouter
{
public:
    LocoNetMessageRouter(LocoNetPort& locoNetPort, MqttPort& mqttPort, Clock& clock,
                          std::vector<std::pair<LocoNetMessageDecoder*, MqttEventEncoder*>> decoders)
        : locoNetPort_(locoNetPort), mqttPort_(mqttPort), clock_(clock), decoders_(std::move(decoders))
    {
    }

    void update();

private:
    void handleMessage(const LocoNetMessage& message);
    void republishAllKnownStatesIfDue();

    LocoNetPort& locoNetPort_;
    MqttPort& mqttPort_;
    Clock& clock_;
    std::vector<std::pair<LocoNetMessageDecoder*, MqttEventEncoder*>> decoders_;
    unsigned long lastRepublishAtMilliseconds_ = 0;

    static constexpr unsigned long kStateRepublishIntervalMs = 30000;
};
