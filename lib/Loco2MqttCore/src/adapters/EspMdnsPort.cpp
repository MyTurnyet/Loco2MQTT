#ifdef ARDUINO

#include "adapters/EspMdnsPort.h"

#include <ESPmDNS.h>

void EspMdnsPort::begin(const std::string& hostname)
{
    MDNS.begin(hostname.c_str());
}

#endif
