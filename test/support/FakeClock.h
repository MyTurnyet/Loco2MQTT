#pragma once

#include "ports/Clock.h"

class FakeClock : public Clock
{
public:
    void setNowMilliseconds(unsigned long value)
    {
        now_ = value;
    }

    unsigned long nowMilliseconds() const override
    {
        return now_;
    }

private:
    unsigned long now_ = 0;
};
