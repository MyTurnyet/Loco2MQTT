#ifdef ARDUINO

#include "adapters/EspMdnsPort.h"

#include <ESPmDNS.h>

bool EspMdnsPort::begin(const std::string& hostname)
{
    return MDNS.begin(hostname.c_str());
}

#endif
