#pragma once

#include "ports/LocoNetPort.h"
#include "ports/MessageLog.h"

class LocoNetMessageLogger
{
public:
    LocoNetMessageLogger(LocoNetPort& port, MessageLog& log) : port_(port), log_(log)
    {
    }

    void update();

private:
    LocoNetPort& port_;
    MessageLog& log_;
};
