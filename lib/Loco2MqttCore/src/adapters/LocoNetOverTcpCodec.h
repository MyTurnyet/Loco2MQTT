#pragma once

#include <optional>
#include <string>

#include "domain/LocoNetMessage.h"

// Pure translation between LocoNetMessage and the LocoNetOverTcp wire
// protocol's line syntax. No Arduino dependency — natively tested.
//
// decodeLine() is TDD'd against lines captured from a real JMRI 5.2
// "Start LocoNet Server" session — see
// docs/decisions/0001-interim-jmri-loconet-over-tcp-transport.md — not
// guessed from the protocol doc alone. It expects an already-line-framed,
// terminator-stripped string (LineStream's job), and returns nullopt for
// anything that isn't a well-formed "RECEIVE <hex bytes>" line: the
// VERSION greeting, SENT OK/ERROR confirmations, and malformed input.
class LocoNetOverTcpCodec
{
public:
    std::string encodeSend(const LocoNetMessage& message) const;
    std::optional<LocoNetMessage> decodeLine(const std::string& line) const;
};
