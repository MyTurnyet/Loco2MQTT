#pragma once

#ifdef ARDUINO

#include <WiFi.h>

#include <string>

class EspWifiPort
{
public:
    EspWifiPort(std::string ssid, std::string password);

    void update();

private:
    std::string ssid_;
    std::string password_;
    unsigned long lastAttemptAtMillis_ = 0;

    static constexpr unsigned long kRetryIntervalMs = 5000;
};

#endif
