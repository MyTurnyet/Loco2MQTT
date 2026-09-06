#pragma once

#include <map>

#include "domain/TurnoutAddress.h"
#include "domain/TurnoutPosition.h"
#include "domain/TurnoutStateChanged.h"
#include "ports/LocoNetMessageDecoder.h"

class TurnoutLocoNetDecoder : public LocoNetMessageDecoder
{
public:
    bool canDecode(uint8_t opcode) const override;
    std::optional<DomainEvent> decode(const LocoNetMessage& message) override;
    std::vector<DomainEvent> allKnownStates() const override;

private:
    std::map<int, TurnoutPosition> lastKnownPosition_;
};
