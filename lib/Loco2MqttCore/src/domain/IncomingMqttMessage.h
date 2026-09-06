#pragma once

#include <string>
#include <utility>

class IncomingMqttMessage
{
public:
    IncomingMqttMessage(std::string topic, std::string payload)
        : topic_(std::move(topic)), payload_(std::move(payload))
    {
    }

    const std::string& topic() const
    {
        return topic_;
    }

    const std::string& payload() const
    {
        return payload_;
    }

private:
    std::string topic_;
    std::string payload_;
};
