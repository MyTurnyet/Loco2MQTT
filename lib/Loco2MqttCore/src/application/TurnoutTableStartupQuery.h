#pragma once

#include <optional>

#include "ports/Clock.h"
#include "ports/LocoNetSendScheduler.h"
#include "turnout/PendingTurnoutStateAcks.h"
#include "turnout/TurnoutStateRequestEncoder.h"

// Once per boot, shortly after startup, walks a fixed turnout address range
// and asks the command station to report each one's current state
// (OPC_SW_STATE) -- a "reset update" so MQTT reflects real layout state as
// soon as the device comes online, rather than waiting for each turnout to
// happen to change. The reply is OPC_LONG_ACK, not OPC_SW_REP as originally
// assumed -- see PendingTurnoutStateAcks and TurnoutLocoNetDecoder for how
// the reply gets matched back to an address and published, through the same
// LocoNetMessageRouter -> TurnoutLocoNetDecoder -> TurnoutMqttEncoder
// pipeline every other decoder uses.
//
// Known limitation: every address's expectation is pushed onto
// PendingTurnoutStateAcks up front, in queryAllTurnouts(), even though the
// actual sends trickle out over ~1 second (kQueryStaggerMs apart). Any
// stray OPC_LONG_ACK switch-state-ack that isn't really one of ours
// arriving during that window (e.g. another client on the bus issuing its
// own OPC_SW_STATE query at the same time) would be misattributed to
// whichever address is next in the queue, corrupting that entry and
// everything behind it. Accepted for now -- narrow window -- not solved here.
class TurnoutTableStartupQuery
{
public:
    TurnoutTableStartupQuery(LocoNetSendScheduler& scheduler, PendingTurnoutStateAcks& pendingAcks, Clock& clock)
        : scheduler_(scheduler), pendingAcks_(pendingAcks), clock_(clock)
    {
    }

    void update();

private:
    bool readyToQuery();
    void queryAllTurnouts();

    LocoNetSendScheduler& scheduler_;
    PendingTurnoutStateAcks& pendingAcks_;
    Clock& clock_;
    TurnoutStateRequestEncoder encoder_;
    std::optional<unsigned long> startedAtMilliseconds_;
    bool queried_ = false;

    // Contiguous range constant for now (matches kOffPulseDelayMs/
    // kLocoNetTxPin precedent elsewhere) -- covers the current DR4018 setup
    // (1-8, 80-88) with room short of it; revisit if that range grows.
    static constexpr int kMinTurnoutAddress = 1;
    static constexpr int kMaxTurnoutAddress = 48;

    // Bus-pacing choice, not a wire-protocol fact -- flagged for
    // verification against real hardware the same way kOffPulseDelayMs is.
    static constexpr unsigned long kQueryStaggerMs = 20;

    // Gives the LocoNet UART and the rest of the bridge time to come up
    // before the first request goes out.
    static constexpr unsigned long kStartupDelayMs = 2000;
};
