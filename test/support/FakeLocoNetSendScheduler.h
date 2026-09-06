#pragma once

#include <utility>
#include <vector>

#include "ports/LocoNetSendScheduler.h"

class FakeLocoNetSendScheduler : public LocoNetSendScheduler
{
public:
    void sendNow(const LocoNetMessage& message) override
    {
        sentNow_.push_back(message);
    }

    void sendAfter(const LocoNetMessage& message, unsigned long delayMilliseconds) override
    {
        scheduled_.emplace_back(message, delayMilliseconds);
    }

    const std::vector<LocoNetMessage>& sentNow() const
    {
        return sentNow_;
    }

    const std::vector<std::pair<LocoNetMessage, unsigned long>>& scheduled() const
    {
        return scheduled_;
    }

private:
    std::vector<LocoNetMessage> sentNow_;
    std::vector<std::pair<LocoNetMessage, unsigned long>> scheduled_;
};
