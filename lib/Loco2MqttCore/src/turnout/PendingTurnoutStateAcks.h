#pragma once

#include <optional>
#include <queue>

#include "domain/TurnoutAddress.h"

// Correlates OPC_LONG_ACK replies with the OPC_SW_STATE requests that
// provoked them -- OPC_LONG_ACK carries no address of its own (see
// TurnoutLocoNetDecoder), so the address has to be inferred from send
// order instead. FIFO, on the empirically-confirmed assumption (a real
// on-hardware capture, 2026-09-14 -- see ADR 0001's addendum) that this
// command station answers OPC_SW_STATE requests one at a time, in order,
// before the next is sent.
//
// TurnoutTableStartupQuery is the sole producer (expect(), one call per
// request it sends); TurnoutLocoNetDecoder is the sole consumer
// (claimNext(), one call per OPC_LONG_ACK switch-state-ack it sees).
class PendingTurnoutStateAcks
{
public:
    void expect(TurnoutAddress address);
    std::optional<TurnoutAddress> claimNext();

private:
    std::queue<int> pendingAddresses_;
};
