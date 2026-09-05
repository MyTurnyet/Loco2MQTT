#pragma once

#include <deque>
#include <vector>

#include "ports/LocoNetPort.h"

class FakeLocoNetPort : public LocoNetPort
{
public:
    void enqueue(const LocoNetMessage& message)
    {
        toReceive_.push_back(message);
    }

    std::optional<LocoNetMessage> receive() override
    {
        if (toReceive_.empty())
        {
            return std::nullopt;
        }
        LocoNetMessage message = toReceive_.front();
        toReceive_.pop_front();
        return message;
    }

    void send(const LocoNetMessage& message) override
    {
        sent_.push_back(message);
    }

    const std::vector<LocoNetMessage>& sent() const
    {
        return sent_;
    }

private:
    std::deque<LocoNetMessage> toReceive_;
    std::vector<LocoNetMessage> sent_;
};
