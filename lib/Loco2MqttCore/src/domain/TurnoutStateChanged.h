#pragma once

#include "domain/TurnoutAddress.h"
#include "domain/TurnoutPosition.h"

class TurnoutStateChanged
{
public:
    TurnoutStateChanged(TurnoutAddress address, TurnoutPosition position)
        : address_(address), position_(position)
    {
    }

    TurnoutAddress address() const
    {
        return address_;
    }

    TurnoutPosition position() const
    {
        return position_;
    }

private:
    TurnoutAddress address_;
    TurnoutPosition position_;
};
