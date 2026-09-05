#pragma once

#ifdef ARDUINO

#include <Arduino.h>

#include "ports/MessageLog.h"

class SerialMessageLog final : public MessageLog
{
public:
    void record(const LocoNetMessage& message) override
    {
        Serial.println(message.describe().c_str());
    }
};

#endif
