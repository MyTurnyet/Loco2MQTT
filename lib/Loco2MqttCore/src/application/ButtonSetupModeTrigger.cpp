#include "ButtonSetupModeTrigger.h"

ButtonSetupModeTrigger::ButtonSetupModeTrigger(DigitalInput& button, Clock& clock, SetupModeRequestStore& requestStore)
    : button_(button), clock_(clock), requestStore_(requestStore)
{
}

bool ButtonSetupModeTrigger::update()
{
    if (!button_.isActive())
    {
        resetHold();
        return false;
    }
    beginHoldIfNeeded();
    return triggerIfHeldLongEnough();
}

void ButtonSetupModeTrigger::resetHold()
{
    isHolding_ = false;
    alreadyTriggered_ = false;
}

void ButtonSetupModeTrigger::beginHoldIfNeeded()
{
    if (isHolding_)
    {
        return;
    }
    isHolding_ = true;
    holdStartMs_ = clock_.nowMilliseconds();
}

bool ButtonSetupModeTrigger::triggerIfHeldLongEnough()
{
    if (alreadyTriggered_)
    {
        return false;
    }
    if (clock_.nowMilliseconds() - holdStartMs_ < kHoldDurationMs)
    {
        return false;
    }
    requestStore_.request();
    alreadyTriggered_ = true;
    return true;
}
