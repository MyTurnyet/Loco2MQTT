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
    void resetHold();
    void beginHoldIfNeeded();
    bool triggerIfHeldLongEnough();

    DigitalInput& button_;
    Clock& clock_;
    SetupModeRequestStore& requestStore_;

    bool isHolding_ = false;
    bool alreadyTriggered_ = false;
    unsigned long holdStartMs_ = 0;

    static constexpr unsigned long kHoldDurationMs = 3000;
};
