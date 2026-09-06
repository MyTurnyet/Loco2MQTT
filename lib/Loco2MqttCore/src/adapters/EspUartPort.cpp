#ifdef ARDUINO

#include "EspUartPort.h"

#include <Arduino.h>

std::optional<std::string> EspUartPort::readLine()
{
    while (Serial.available() > 0)
    {
        const char c = static_cast<char>(Serial.read());
        if (c == '\n')
        {
            std::string line = buffer_;
            buffer_.clear();
            if (!line.empty() && line.back() == '\r')
            {
                line.pop_back();
            }
            return line;
        }
        if (buffer_.size() < kMaxBufferedBytes)
        {
            buffer_ += c;
        }
    }
    return std::nullopt;
}

void EspUartPort::writeLine(const std::string& line)
{
    Serial.println(line.c_str());
}

#endif
