#pragma once

class RebootTrigger
{
public:
    virtual ~RebootTrigger() = default;
    virtual void reboot() = 0;
};
