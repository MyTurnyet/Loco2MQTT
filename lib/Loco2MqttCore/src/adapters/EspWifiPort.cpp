#ifdef ARDUINO

#include "adapters/EspWifiPort.h"

EspWifiPort::EspWifiPort(std::string ssid, std::string password)
    : ssid_(std::move(ssid)), password_(std::move(password))
{
}

void EspWifiPort::update()
{
    if (WiFi.status() == WL_CONNECTED)
    {
        return;
    }
    const unsigned long now = millis();
    if (now - lastAttemptAtMillis_ < kRetryIntervalMs)
    {
        return;
    }
    lastAttemptAtMillis_ = now;
    WiFi.begin(ssid_.c_str(), password_.c_str());
}

#endif
