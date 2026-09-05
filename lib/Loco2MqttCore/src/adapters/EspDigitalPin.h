#pragma once

#ifdef ARDUINO

#include <Arduino.h>

#include "ports/DigitalPin.h"

class EspDigitalPin final : public DigitalPin
{
public:
    explicit EspDigitalPin(int pin) : pin_(pin)
    {
        pinMode(pin_, OUTPUT);
        digitalWrite(pin_, LOW);
    }

    void write(Level level) override
    {
        digitalWrite(pin_, level == Level::High ? HIGH : LOW);
    }

private:
    int pin_;
};

#endif
