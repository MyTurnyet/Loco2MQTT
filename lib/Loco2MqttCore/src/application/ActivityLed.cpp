#include "ActivityLed.h"

ActivityLed::ActivityLed(DigitalPin& pin, Clock& clock, unsigned long flashDurationMs)
    : pin_(pin), clock_(clock), flashDurationMs_(flashDurationMs)
{
}

void ActivityLed::flash()
{
    pin_.write(Level::High);
    litSinceMs_ = clock_.nowMilliseconds();
    lit_ = true;
}

void ActivityLed::update()
{
    if (lit_ && dueToTurnOff())
    {
        pin_.write(Level::Low);
        lit_ = false;
    }
}

bool ActivityLed::dueToTurnOff() const
{
    return clock_.nowMilliseconds() - litSinceMs_ >= flashDurationMs_;
}
