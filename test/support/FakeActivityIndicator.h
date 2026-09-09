#pragma once

#include "ports/ActivityIndicator.h"

class FakeActivityIndicator : public ActivityIndicator
{
public:
    void flash() override
    {
        flashCallCount_++;
    }

    int flashCallCount() const
    {
        return flashCallCount_;
    }

private:
    int flashCallCount_ = 0;
};
