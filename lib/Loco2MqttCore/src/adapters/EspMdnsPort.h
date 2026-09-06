#pragma once

#ifdef ARDUINO

#include <string>

class EspMdnsPort
{
public:
    void begin(const std::string& hostname);
};

#endif
