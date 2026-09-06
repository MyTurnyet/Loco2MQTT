#pragma once

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ports/LocoNetEncoder.h"
#include "ports/MqttCommandDecoder.h"
#include "ports/MqttPort.h"

class MqttCommandRouter
{
public:
    MqttCommandRouter(MqttPort& mqttPort, LocoNetSendScheduler& scheduler,
                       std::vector<std::pair<MqttCommandDecoder*, LocoNetEncoder*>> decoders)
        : mqttPort_(mqttPort), scheduler_(scheduler), decoders_(std::move(decoders))
    {
    }

    void update();

private:
    void handleMessage(const IncomingMqttMessage& message);

    MqttPort& mqttPort_;
    LocoNetSendScheduler& scheduler_;
    std::vector<std::pair<MqttCommandDecoder*, LocoNetEncoder*>> decoders_;
};
