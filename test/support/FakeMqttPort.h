#pragma once

#include <deque>
#include <vector>

#include "ports/MqttPort.h"

class FakeMqttPort : public MqttPort
{
public:
    void enqueueCommand(const IncomingMqttMessage& message)
    {
        toReceive_.push_back(message);
    }

    std::optional<IncomingMqttMessage> receiveCommand() override
    {
        if (toReceive_.empty())
        {
            return std::nullopt;
        }
        IncomingMqttMessage message = toReceive_.front();
        toReceive_.pop_front();
        return message;
    }

    void publish(const MqttMessage& message) override
    {
        published_.push_back(message);
    }

    const std::vector<MqttMessage>& published() const
    {
        return published_;
    }

private:
    std::deque<IncomingMqttMessage> toReceive_;
    std::vector<MqttMessage> published_;
};
