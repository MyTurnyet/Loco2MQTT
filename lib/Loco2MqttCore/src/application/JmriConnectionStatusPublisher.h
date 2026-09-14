#pragma once

#include <optional>

#include "ports/Clock.h"
#include "ports/LineStream.h"
#include "ports/MqttPort.h"

// Surfaces the interim JMRI LocoNet-over-TCP transport's connection health
// on MQTT, the same way turnout state is surfaced: publish on every actual
// change, plus a periodic republish as the reliability backstop for late
// subscribers (PicoMQTT's broker mode doesn't honor the retained flag —
// see PicoMqttPort::publish()). See
// docs/decisions/0001-interim-jmri-loconet-over-tcp-transport.md.
class JmriConnectionStatusPublisher
{
public:
    JmriConnectionStatusPublisher(LineStream& stream, MqttPort& mqttPort, Clock& clock);

    void update();

private:
    void publishIfChanged();
    void republishIfDue();
    void publish();

    LineStream& stream_;
    MqttPort& mqttPort_;
    Clock& clock_;
    std::optional<bool> lastKnownConnected_;
    unsigned long lastPublishedAtMs_ = 0;

    static constexpr unsigned long kRepublishIntervalMs = 30000;
};