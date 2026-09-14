#pragma once

#include <optional>

#include "ports/LineStream.h"
#include "ports/LocoNetSendScheduler.h"
#include "turnout/TurnoutStateRequestEncoder.h"

// On every transition from not-connected to connected on the interim JMRI
// transport (see docs/decisions/0001-interim-jmri-loconet-over-tcp-transport.md),
// walks a fixed turnout address range and asks the command station to
// report each one's current state (OPC_SW_STATE) — a "reset update" so MQTT
// reflects real layout state as soon as the device comes online, rather
// than waiting for each turnout to happen to change. Replies arrive as
// ordinary OPC_SW_REP traffic and flow through the existing
// LocoNetMessageRouter -> TurnoutLocoNetDecoder -> TurnoutMqttEncoder
// pipeline unchanged.
//
// Ties to LineStream, not LocoNetPort, on purpose -- same precedent as
// JmriConnectionStatusPublisher: "connected" is a concept the interim
// TCP transport has and the real electrical bus doesn't. When ADR 0001
// reverts to LocoNetEsp32Port, this class's trigger needs rethinking (most
// likely: fire once, unconditionally, right after setupMqttBridge()) --
// deliberately not solved here, per the ADR's own "no runtime-configurable
// transport selection" scope discipline.
class TurnoutTableStartupQuery
{
public:
    TurnoutTableStartupQuery(LineStream& stream, LocoNetSendScheduler& scheduler)
        : stream_(stream), scheduler_(scheduler)
    {
    }

    void update();

private:
    bool justConnected();
    void queryAllTurnouts();

    LineStream& stream_;
    LocoNetSendScheduler& scheduler_;
    TurnoutStateRequestEncoder encoder_;
    std::optional<bool> lastKnownConnected_;

    // Contiguous range constant for now (matches kOffPulseDelayMs/
    // kLocoNetTxPin precedent elsewhere) -- covers the current DR4018 setup
    // (1-8, 80-88) with room short of it; revisit if that range grows.
    static constexpr int kMinTurnoutAddress = 1;
    static constexpr int kMaxTurnoutAddress = 48;

    // Bus-pacing choice, not a wire-protocol fact -- flagged for
    // verification against real hardware the same way kOffPulseDelayMs is.
    static constexpr unsigned long kQueryStaggerMs = 20;
};
