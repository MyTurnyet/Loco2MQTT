#include "application/TurnoutTableStartupQuery.h"

#include "domain/TurnoutAddress.h"

void TurnoutTableStartupQuery::update()
{
    if (justConnected())
    {
        queryAllTurnouts();
    }
}

bool TurnoutTableStartupQuery::justConnected()
{
    const bool connected = stream_.isConnected();
    const bool wasDisconnected = !lastKnownConnected_.has_value() || !*lastKnownConnected_;
    lastKnownConnected_ = connected;
    return connected && wasDisconnected;
}

void TurnoutTableStartupQuery::queryAllTurnouts()
{
    unsigned long delayMilliseconds = 0;
    for (int address = kMinTurnoutAddress; address <= kMaxTurnoutAddress; ++address)
    {
        scheduler_.sendAfter(encoder_.encode(TurnoutAddress(address)), delayMilliseconds);
        delayMilliseconds += kQueryStaggerMs;
    }
}
