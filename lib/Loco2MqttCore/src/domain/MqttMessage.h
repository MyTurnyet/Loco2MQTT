#pragma once

#include <string>
#include <utility>

class MqttMessage
{
public:
    MqttMessage(std::string topic, std::string payload, bool retained)
        : topic_(std::move(topic)), payload_(std::move(payload)), retained_(retained)
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

    bool retained() const
    {
        return retained_;
    }

private:
    std::string topic_;
    std::string payload_;
    bool retained_;
};
