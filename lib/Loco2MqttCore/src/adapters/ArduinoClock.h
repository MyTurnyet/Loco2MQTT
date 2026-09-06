#pragma once

#ifdef ARDUINO

#include <Arduino.h>

#include "ports/Clock.h"

class ArduinoClock final : public Clock
{
public:
    unsigned long nowMilliseconds() const override
    {
        return millis();
    }
};

#endif
