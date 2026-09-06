#pragma once

#ifdef ARDUINO

#include "ports/SetupModeRequestStore.h"

class NvsSetupModeRequestStore final : public SetupModeRequestStore
{
public:
    void request() override;
    bool consumeIfRequested() override;
};

#endif
