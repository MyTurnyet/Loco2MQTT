#pragma once

#include <vector>

#include "ports/MessageLog.h"

class FakeMessageLog : public MessageLog
{
public:
    void record(const LocoNetMessage& message) override
    {
        recorded_.push_back(message);
    }

    const std::vector<LocoNetMessage>& recorded() const
    {
        return recorded_;
    }

private:
    std::vector<LocoNetMessage> recorded_;
};
