#ifdef ARDUINO

#include "adapters/PicoMqttPort.h"

PicoMqttPort::PicoMqttPort()
{
    server_.subscribe("loconet/+/+/set", [this](const char* topic, const char* payload)
    {
        if (pendingCommands_.size() >= kMaxPendingCommands)
        {
            pendingCommands_.pop_front();
        }
        pendingCommands_.emplace_back(std::string(topic), std::string(payload));
    });
}

void PicoMqttPort::begin()
{
    server_.begin();
}

void PicoMqttPort::update()
{
    server_.loop();
}

void PicoMqttPort::publish(const MqttMessage& message)
{
    // PicoMQTT's broker mode ignores the retained flag and only supports
    // QoS 0 -- message.retained() has no effect here. LocoNetMessageRouter's
    // periodic full state re-publish is the actual reliability mechanism
    // for late subscribers; see the design spec's "MQTT contract" section.
    // Logged for the same reason SerialMessageLog logs every inbound
    // LocoNet message: on-hardware validation of this transport has never
    // been done (see docs/decisions/0001's "Implementation status"), so
    // this is the only visibility into whether a decoded turnout event
    // actually reaches this call.
    Serial.print("MQTT publish: ");
    Serial.print(message.topic().c_str());
    Serial.print(" = ");
    Serial.println(message.payload().c_str());
    server_.publish(message.topic().c_str(), message.payload().c_str());
}

std::optional<IncomingMqttMessage> PicoMqttPort::receiveCommand()
{
    if (pendingCommands_.empty())
    {
        return std::nullopt;
    }
    IncomingMqttMessage message = pendingCommands_.front();
    pendingCommands_.pop_front();
    return message;
}

#endif
