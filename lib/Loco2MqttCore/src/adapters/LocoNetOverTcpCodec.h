#pragma once

#include <string>

#include "domain/LocoNetMessage.h"

// Pure translation between LocoNetMessage and the LocoNetOverTcp wire
// protocol's line syntax. No Arduino dependency — natively tested.
//
// decodeLine() is intentionally not yet present: the inbound line format
// (what JMRI's "Start LocoNet Server" actually sends) is being confirmed
// empirically before it's implemented — see
// docs/decisions/0001-interim-jmri-loconet-over-tcp-transport.md.
class LocoNetOverTcpCodec
{
public:
    std::string encodeSend(const LocoNetMessage& message) const;
};
