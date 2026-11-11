#pragma once

#ifdef ARDUINO

#include <Arduino.h>

#include "ports/MessageLog.h"

class SerialMessageLog final : public MessageLog
{
public:
    // `direction` labels each line ("RX"/"TX") so received and sent traffic
    // can be told apart on the console.
    explicit SerialMessageLog(const char* direction) : direction_(direction)
    {
    }

    void record(const LocoNetMessage& message) override
    {
        Serial.print(direction_);
        Serial.print(' ');
        Serial.println(message.describe().c_str());
    }

private:
    const char* direction_;
};

#endif
