#pragma once

#ifdef ARDUINO

#include <Arduino.h>

#include "ports/DigitalInput.h"

class EspDigitalInput final : public DigitalInput
{
public:
    explicit EspDigitalInput(int pin) : pin_(pin)
    {
        pinMode(pin_, INPUT_PULLUP);
    }

    bool isActive() const override
    {
        return digitalRead(pin_) == LOW;
    }

private:
    int pin_;
};

#endif
