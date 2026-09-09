#pragma once

#include "ports/ActivityIndicator.h"
#include "ports/Clock.h"
#include "ports/DigitalPin.h"

// Drives a DigitalPin High for a fixed window every time flash() is
// called, then update() (ticked from loop()) drops it back Low once the
// window elapses. A flash() that arrives while already lit restarts the
// window instead of queuing a second pulse -- under LocoNet traffic
// bursts this reads as one held flash rather than an imperceptible
// flicker, which is the whole point of an activity indicator.
class ActivityLed final : public ActivityIndicator
{
public:
    ActivityLed(DigitalPin& pin, Clock& clock, unsigned long flashDurationMs = kDefaultFlashDurationMs);

    void flash() override;
    void update();

private:
    bool dueToTurnOff() const;

    DigitalPin& pin_;
    Clock& clock_;
    unsigned long flashDurationMs_;
    unsigned long litSinceMs_ = 0;
    bool lit_ = false;

    static constexpr unsigned long kDefaultFlashDurationMs = 40;
};
