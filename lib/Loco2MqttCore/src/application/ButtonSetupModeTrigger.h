#pragma once

#include "ports/Clock.h"
#include "ports/DigitalInput.h"
#include "ports/SetupModeRequestStore.h"

class ButtonSetupModeTrigger
{
public:
    ButtonSetupModeTrigger(DigitalInput& button, Clock& clock, SetupModeRequestStore& requestStore);

    bool update();

private:
    bool handleActive();
    bool handleInactive();
    bool finishReleaseIfSettled();
    bool triggerRequest();

    DigitalInput& button_;
    Clock& clock_;
    SetupModeRequestStore& requestStore_;

    bool holding_ = false;
    unsigned long holdStartMs_ = 0;
    bool releasing_ = false;
    unsigned long releaseStartMs_ = 0;

    static constexpr unsigned long kHoldDurationMs = 3000;
    static constexpr unsigned long kReleaseSettleMs = 50;
};
