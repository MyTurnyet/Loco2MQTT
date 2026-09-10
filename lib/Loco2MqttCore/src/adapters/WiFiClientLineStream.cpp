#ifdef ARDUINO

#include "WiFiClientLineStream.h"

#include <utility>

WiFiClientLineStream::WiFiClientLineStream(std::string host, uint16_t port) : host_(std::move(host)), port_(port)
{
}

std::optional<std::string> WiFiClientLineStream::readLine()
{
    reconnectIfDue();
    while (client_.available() > 0)
    {
        auto line = assembler_.feed(static_cast<char>(client_.read()));
        if (line.has_value())
        {
            return line;
        }
    }
    return std::nullopt;
}

void WiFiClientLineStream::writeLine(const std::string& line)
{
    reconnectIfDue();
    if (!client_.connected())
    {
        return;
    }
    client_.print(line.c_str());
    client_.print('\r');
}

bool WiFiClientLineStream::isConnected() const
{
    return client_.connected();
}

void WiFiClientLineStream::reconnectIfDue()
{
    if (client_.connected())
    {
        return;
    }
    if (millis() - lastConnectAttemptAtMillis_ < kReconnectIntervalMs)
    {
        return;
    }
    lastConnectAttemptAtMillis_ = millis();
    client_.connect(host_.c_str(), port_);
}

#endif
