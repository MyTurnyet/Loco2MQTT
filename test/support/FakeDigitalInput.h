#pragma once

#include "ports/DigitalInput.h"

class FakeDigitalInput : public DigitalInput
{
public:
    void setActive(bool active)
    {
        active_ = active;
    }

    bool isActive() const override
    {
        return active_;
    }

private:
    bool active_ = false;
};
