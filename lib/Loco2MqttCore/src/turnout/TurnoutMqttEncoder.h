#pragma once

#include "domain/TurnoutStateChanged.h"
#include "ports/MqttEventEncoder.h"

class TurnoutMqttEncoder : public MqttEventEncoder
{
public:
    MqttMessage encode(const DomainEvent& event) const override;
};
