#pragma once

#include "adapters/LocoNetOverTcpCodec.h"
#include "ports/LineStream.h"
#include "ports/LocoNetPort.h"

// LocoNetPort backed by JMRI's LocoNetOverTcp "Start LocoNet Server"
// feature instead of the electrical bus. Interim transport — see
// docs/decisions/0001-interim-jmri-loconet-over-tcp-transport.md.
//
// Owns a LocoNetOverTcpCodec by value rather than injecting it as an
// interface: there is exactly one wire format here (unlike LocoNetPort,
// which has multiple real/fake implementations), so there's nothing to
// substitute.
class LocoNetOverTcpPort final : public LocoNetPort
{
public:
    explicit LocoNetOverTcpPort(LineStream& stream) : stream_(stream)
    {
    }

    std::optional<LocoNetMessage> receive() override;
    void send(const LocoNetMessage& message) override;

private:
    std::optional<LocoNetMessage> decodeNextMessage(const std::string& line);

    LineStream& stream_;
    LocoNetOverTcpCodec codec_;
};
