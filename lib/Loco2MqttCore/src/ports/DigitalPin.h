#pragma once

#include "domain/Level.h"

class DigitalPin
{
public:
    virtual ~DigitalPin() = default;
    virtual void write(Level level) = 0;
};
