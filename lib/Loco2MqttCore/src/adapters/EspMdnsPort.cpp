#ifdef ARDUINO

#include "adapters/EspMdnsPort.h"

#include <ESPmDNS.h>

bool EspMdnsPort::begin(const std::string& hostname)
{
    const unsigned long now = millis();
    if (now - lastAttemptAtMillis_ < kRetryIntervalMs)
    {
        return false;
    }
    lastAttemptAtMillis_ = now;
    return MDNS.begin(hostname.c_str());
}

#endif
