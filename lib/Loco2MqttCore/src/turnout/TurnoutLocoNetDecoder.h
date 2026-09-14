#pragma once

#include <map>

#include "domain/TurnoutAddress.h"
#include "domain/TurnoutPosition.h"
#include "domain/TurnoutStateChanged.h"
#include "ports/LocoNetMessageDecoder.h"
#include "turnout/PendingTurnoutStateAcks.h"

class TurnoutLocoNetDecoder : public LocoNetMessageDecoder
{
public:
    explicit TurnoutLocoNetDecoder(PendingTurnoutStateAcks& pendingAcks) : pendingAcks_(pendingAcks)
    {
    }

    bool canDecode(uint8_t opcode) const override;
    std::optional<DomainEvent> decode(const LocoNetMessage& message) override;
    std::vector<DomainEvent> allKnownStates() const override;

private:
    std::optional<DomainEvent> decodeSwitchMessage(const LocoNetMessage& message);
    std::optional<DomainEvent> decodeStateAck(const LocoNetMessage& message);
    std::optional<DomainEvent> emitIfChanged(int address, TurnoutPosition position);
    bool shouldEmitPositionChange(int address, TurnoutPosition position);

    PendingTurnoutStateAcks& pendingAcks_;
    std::map<int, TurnoutPosition> lastKnownPosition_;
};
