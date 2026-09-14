#pragma once

#include <optional>

#include "ports/Clock.h"
#include "ports/LineStream.h"
#include "ports/LocoNetSendScheduler.h"
#include "turnout/PendingTurnoutStateAcks.h"
#include "turnout/TurnoutStateRequestEncoder.h"

// On every transition from not-connected to connected on the interim JMRI
// transport (see docs/decisions/0001-interim-jmri-loconet-over-tcp-transport.md),
// walks a fixed turnout address range and asks the command station to
// report each one's current state (OPC_SW_STATE) — a "reset update" so MQTT
// reflects real layout state as soon as the device comes online, rather
// than waiting for each turnout to happen to change. The reply is
// OPC_LONG_ACK, not OPC_SW_REP as originally assumed — see
// PendingTurnoutStateAcks and TurnoutLocoNetDecoder for how the reply gets
// matched back to an address and published, through the same
// LocoNetMessageRouter -> TurnoutLocoNetDecoder -> TurnoutMqttEncoder
// pipeline every other decoder uses.
//
// Debounced on purpose: WiFiClientLineStream::isConnected() (real hardware,
// 2026-09-14) was observed re-triggering this query repeatedly rather than
// once per genuine reconnect — WiFiClient::connected() on the ESP32 Arduino
// core is known to report transient false negatives even on a healthy
// socket, and WiFiClientLineStream's own reconnectIfDue() then acts on
// that false reading and forces a real stop()/connect() cycle, which is a
// genuine (if spurious) disconnect/reconnect as far as this class can
// tell. Rather than trust the very first "connected" reading after a
// "disconnected" one, this waits for isConnected() to stay continuously
// true for kStableConnectionMs before treating it as a real reconnect —
// any blip back to false during that window resets the wait. A duration,
// not a protocol fact -- flagged for verification the same way
// kQueryStaggerMs is; widen it if brief flapping still causes a refire.
//
// Known limitation: every address's expectation is pushed onto
// PendingTurnoutStateAcks up front, in queryAllTurnouts(), even though the
// actual sends trickle out over ~1 second (kQueryStaggerMs apart). Any
// stray OPC_LONG_ACK switch-state-ack that isn't really one of ours
// arriving during that window (e.g. another client on the bus issuing its
// own OPC_SW_STATE query at the same time) would be misattributed to
// whichever address is next in the queue, corrupting that entry and
// everything behind it. Accepted for now — narrow window, single expected
// JMRI client in practice — not solved here.
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
    TurnoutTableStartupQuery(LineStream& stream, LocoNetSendScheduler& scheduler, PendingTurnoutStateAcks& pendingAcks,
                              Clock& clock)
        : stream_(stream), scheduler_(scheduler), pendingAcks_(pendingAcks), clock_(clock)
    {
    }

    void update();

private:
    void resetForNextConnection();
    bool readyToQuery();
    void queryAllTurnouts();

    LineStream& stream_;
    LocoNetSendScheduler& scheduler_;
    PendingTurnoutStateAcks& pendingAcks_;
    Clock& clock_;
    TurnoutStateRequestEncoder encoder_;
    std::optional<unsigned long> connectedSinceMilliseconds_;
    bool queriedThisConnection_ = false;

    // Contiguous range constant for now (matches kOffPulseDelayMs/
    // kLocoNetTxPin precedent elsewhere) -- covers the current DR4018 setup
    // (1-8, 80-88) with room short of it; revisit if that range grows.
    static constexpr int kMinTurnoutAddress = 1;
    static constexpr int kMaxTurnoutAddress = 48;

    // Bus-pacing choice, not a wire-protocol fact -- flagged for
    // verification against real hardware the same way kOffPulseDelayMs is.
    static constexpr unsigned long kQueryStaggerMs = 20;

    // See the debounce note above.
    static constexpr unsigned long kStableConnectionMs = 2000;
};
