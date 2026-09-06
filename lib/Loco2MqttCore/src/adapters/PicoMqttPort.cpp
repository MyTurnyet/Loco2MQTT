#ifdef ARDUINO

#include "adapters/PicoMqttPort.h"

PicoMqttPort::PicoMqttPort()
{
    server_.subscribe("loconet/+/+/set", [this](const char* topic, const char* payload)
    {
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
