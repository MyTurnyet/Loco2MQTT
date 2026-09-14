#include "application/TurnoutTableStartupQuery.h"

#include "domain/TurnoutAddress.h"

void TurnoutTableStartupQuery::update()
{
    if (!stream_.isConnected())
    {
        resetForNextConnection();
        return;
    }
    if (readyToQuery())
    {
        queryAllTurnouts();
        queriedThisConnection_ = true;
    }
}

void TurnoutTableStartupQuery::resetForNextConnection()
{
    connectedSinceMilliseconds_.reset();
    queriedThisConnection_ = false;
}

bool TurnoutTableStartupQuery::readyToQuery()
{
    if (!connectedSinceMilliseconds_.has_value())
    {
        connectedSinceMilliseconds_ = clock_.nowMilliseconds();
    }
    if (queriedThisConnection_)
    {
        return false;
    }
    return clock_.nowMilliseconds() - *connectedSinceMilliseconds_ >= kStableConnectionMs;
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
