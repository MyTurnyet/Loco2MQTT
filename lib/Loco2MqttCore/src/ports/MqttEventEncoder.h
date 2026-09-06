#pragma once

#include "domain/DomainEvent.h"
#include "domain/MqttMessage.h"

class MqttEventEncoder
{
public:
    virtual ~MqttEventEncoder() = default;
    virtual MqttMessage encode(const DomainEvent& event) const = 0;
};
