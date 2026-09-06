#pragma once

#include "ports/SetupModeRequestStore.h"

class FakeSetupModeRequestStore : public SetupModeRequestStore
{
public:
    void request() override
    {
        requested_ = true;
    }

    bool consumeIfRequested() override
    {
        const bool wasRequested = requested_;
        requested_ = false;
        return wasRequested;
    }

private:
    bool requested_ = false;
};
