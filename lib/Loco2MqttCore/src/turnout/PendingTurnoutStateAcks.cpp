#include "turnout/PendingTurnoutStateAcks.h"

void PendingTurnoutStateAcks::expect(TurnoutAddress address)
{
    pendingAddresses_.push(address.value());
}

std::optional<TurnoutAddress> PendingTurnoutStateAcks::claimNext()
{
    if (pendingAddresses_.empty())
    {
        return std::nullopt;
    }
    const int address = pendingAddresses_.front();
    pendingAddresses_.pop();
    return TurnoutAddress(address);
}
