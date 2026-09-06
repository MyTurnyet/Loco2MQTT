#pragma once

#ifdef ARDUINO

#include <Arduino.h>

#include "ports/RebootTrigger.h"

class EspRebootTrigger final : public RebootTrigger
{
public:
    void reboot() override
    {
        ESP.restart();
    }
};

#endif
