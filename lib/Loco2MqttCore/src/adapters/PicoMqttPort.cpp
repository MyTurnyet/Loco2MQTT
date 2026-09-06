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
