#pragma once

#include "domain/LocoNetMessage.h"
#include "domain/TurnoutAddress.h"

// Encodes an OPC_SW_STATE ("request switch state") LocoNet message for a
// given address — a read-only query, distinct from TurnoutLocoNetEncoder's
// OPC_SW_REQ (which commands the turnout to move). The command station
// answers either opcode with the same OPC_SW_REP TurnoutLocoNetDecoder
// already decodes, so no decoder changes are needed to receive the reply.
class TurnoutStateRequestEncoder
{
public:
    LocoNetMessage encode(TurnoutAddress address) const;
};
