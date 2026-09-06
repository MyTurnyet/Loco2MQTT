#pragma once

#include "ports/RebootTrigger.h"

class FakeRebootTrigger : public RebootTrigger
{
public:
    void reboot() override
    {
        rebootCount_++;
    }

    int rebootCount() const
    {
        return rebootCount_;
    }

private:
    int rebootCount_ = 0;
};
