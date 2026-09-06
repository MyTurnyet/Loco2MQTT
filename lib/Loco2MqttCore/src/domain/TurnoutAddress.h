#pragma once

class TurnoutAddress
{
public:
    explicit TurnoutAddress(int address) : address_(address)
    {
    }

    int value() const
    {
        return address_;
    }

private:
    int address_;
};
