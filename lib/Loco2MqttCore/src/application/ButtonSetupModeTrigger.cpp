#include "ButtonSetupModeTrigger.h"

ButtonSetupModeTrigger::ButtonSetupModeTrigger(DigitalInput& button, Clock& clock, SetupModeRequestStore& requestStore)
    : button_(button), clock_(clock), requestStore_(requestStore)
{
}

bool ButtonSetupModeTrigger::update()
{
    if (button_.isActive())
    {
        return handleActive();
    }
    return handleInactive();
}

bool ButtonSetupModeTrigger::handleActive()
{
    if (!holding_)
    {
        holding_ = true;
        holdStartMs_ = clock_.nowMilliseconds();
    }
    // Contact bounce: the button never actually left, so cancel any
    // release-in-progress and let the original press time stand.
    releasing_ = false;
    return false;
}

bool ButtonSetupModeTrigger::handleInactive()
{
    if (!holding_)
    {
        return false;
    }
    if (!releasing_)
    {
        releasing_ = true;
        releaseStartMs_ = clock_.nowMilliseconds();
        return false;
    }
    return finishReleaseIfSettled();
}

bool ButtonSetupModeTrigger::finishReleaseIfSettled()
{
    if (clock_.nowMilliseconds() - releaseStartMs_ < kReleaseSettleMs)
    {
        return false;
    }
    const unsigned long heldFor = releaseStartMs_ - holdStartMs_;
    holding_ = false;
    releasing_ = false;
    return heldFor >= kHoldDurationMs && triggerRequest();
}

bool ButtonSetupModeTrigger::triggerRequest()
{
    requestStore_.request();
    return true;
}
