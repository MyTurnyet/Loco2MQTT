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
};

#endif
