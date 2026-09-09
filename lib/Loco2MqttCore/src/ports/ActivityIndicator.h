#pragma once

class ActivityIndicator
{
public:
    virtual ~ActivityIndicator() = default;
    virtual void flash() = 0;
};
