#pragma once

#ifdef ARDUINO

#include <string>

class EspMdnsPort
{
public:
    // Returns false on failure (e.g. mdns_init() or mdns_hostname_set()
    // failing internally) so the caller can retry on a later tick instead
    // of latching a permanent false success.
    bool begin(const std::string& hostname);
};

#endif
