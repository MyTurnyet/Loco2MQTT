#ifdef ARDUINO

#include "EspUartPort.h"

#include <Arduino.h>

EspUartPort::EspUartPort() : assembler_(kMaxBufferedBytes)
{
}

std::optional<std::string> EspUartPort::readLine()
{
    while (Serial.available() > 0)
    {
        auto line = assembler_.feed(static_cast<char>(Serial.read()));
        if (line.has_value())
        {
            return line;
        }
    }
    return std::nullopt;
}

void EspUartPort::writeLine(const std::string& line)
{
    Serial.println(line.c_str());
}

#endif
