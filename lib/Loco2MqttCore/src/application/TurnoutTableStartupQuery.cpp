#include "application/TurnoutTableStartupQuery.h"

#include "domain/TurnoutAddress.h"

void TurnoutTableStartupQuery::update()
{
    if (readyToQuery())
    {
        queryAllTurnouts();
        queried_ = true;
    }
}

bool TurnoutTableStartupQuery::readyToQuery()
{
    if (!startedAtMilliseconds_.has_value())
    {
        startedAtMilliseconds_ = clock_.nowMilliseconds();
    }
    if (queried_)
    {
        return false;
    }
    return clock_.nowMilliseconds() - *startedAtMilliseconds_ >= kStartupDelayMs;
}

void TurnoutTableStartupQuery::queryAllTurnouts()
{
    unsigned long delayMilliseconds = 0;
    for (int address = kMinTurnoutAddress; address <= kMaxTurnoutAddress; ++address)
    {
        pendingAcks_.expect(TurnoutAddress(address));
        scheduler_.sendAfter(encoder_.encode(TurnoutAddress(address)), delayMilliseconds);
        delayMilliseconds += kQueryStaggerMs;
    }
}
