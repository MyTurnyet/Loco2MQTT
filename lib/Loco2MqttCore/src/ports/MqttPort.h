#pragma once

#include <optional>

#include "domain/IncomingMqttMessage.h"
#include "domain/MqttMessage.h"

class MqttPort
{
public:
    virtual ~MqttPort() = default;
    virtual void publish(const MqttMessage& message) = 0;
    virtual std::optional<IncomingMqttMessage> receiveCommand() = 0;
};
