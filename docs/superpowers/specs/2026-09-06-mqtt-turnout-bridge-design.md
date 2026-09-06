# MQTT Turnout Bridge — Design

## Overview

This is the first MQTT-bridge sub-project for Loco2MQTT. It delivers a
complete, real-hardware vertical slice: the ESP32 connects to WiFi using the
credentials already captured by the commissioning sub-project, runs
[PicoMQTT](https://github.com/mlesniak/PicoMQTT) on-device as the MQTT
broker (no external broker anywhere in the system), and bridges Digitrax
LocoNet turnout traffic to and from MQTT in both directions.

This document specializes the architecture context the user supplied
separately (routers/decoders/encoders, the turnout device-type contract,
the two-tier testing strategy) into concrete decisions for this
implementation. It does not repeat that document's content in full; read it
alongside this one.

## Engineering principles (carried over, non-negotiable)

- TDD, no mocking frameworks — real objects or hand-written fakes only.
- Methods ≤ 8 lines, cognitive complexity < 4.
- Constructor-based dependency injection; no statics, no singletons.
- Composition over inheritance.
- Immutable value objects for all domain data.
- Ask, don't tell.
- Two-tier testing: native unit tests for all pure logic (routers, decoders,
  encoders, domain objects, the send scheduler); on-device build-checks only
  for the two genuinely hardware-bound adapters (WiFi, PicoMQTT broker) plus
  the existing LocoNet transceiver.

## Scope

**In scope:** WiFi connects on boot using stored commissioning credentials;
PicoMQTT runs on-device as the broker; the turnout device type works
end-to-end, LocoNet → MQTT and MQTT → LocoNet, against real hardware.

**Out of scope:** any device type other than turnout (sensor, transponder,
etc. — explicitly future work per the architecture doc); any PC/JMRI-side
tooling; changes to the wireless-setup commissioning flow itself (already
shipped).

## Domain objects

```cpp
// domain/TurnoutAddress.h — immutable, deliberately unvalidated
class TurnoutAddress {
public:
    explicit TurnoutAddress(int address);
    int value() const;
private:
    int address_;
};

// domain/TurnoutPosition.h
enum class TurnoutPosition { Closed, Thrown };

// domain/TurnoutStateChanged.h — event (LocoNet → MQTT)
class TurnoutStateChanged {
public:
    TurnoutStateChanged(TurnoutAddress address, TurnoutPosition position);
    TurnoutAddress address() const;
    TurnoutPosition position() const;
private:
    TurnoutAddress address_;
    TurnoutPosition position_;
};

// domain/SetTurnoutPosition.h — command (MQTT → LocoNet), same shape as above

// domain/DomainEvent.h / domain/DomainCommand.h
using DomainEvent = std::variant<TurnoutStateChanged>;
using DomainCommand = std::variant<SetTurnoutPosition>;

// domain/MqttMessage.h — outbound
class MqttMessage {
public:
    MqttMessage(std::string topic, std::string payload, bool retained);
    const std::string& topic() const;
    const std::string& payload() const;
    bool retained() const;
private:
    std::string topic_;
    std::string payload_;
    bool retained_;
};

// domain/IncomingMqttMessage.h — inbound (no retained flag; not meaningful on receipt)
class IncomingMqttMessage {
public:
    IncomingMqttMessage(std::string topic, std::string payload);
    const std::string& topic() const;
    const std::string& payload() const;
private:
    std::string topic_;
    std::string payload_;
};

// domain/PendingLocoNetSend.h — one scheduled future send
class PendingLocoNetSend {
public:
    PendingLocoNetSend(LocoNetMessage message, unsigned long dueAtMilliseconds);
    const LocoNetMessage& message() const;
    unsigned long dueAtMilliseconds() const;
private:
    LocoNetMessage message_;
    unsigned long dueAtMilliseconds_;
};
```

**`TurnoutAddress` validates nothing at construction, deliberately:** every
call site that constructs one already guarantees range 1..2048 by
construction — `TurnoutLocoNetDecoder`'s bit math can only ever produce
`((0x0F << 7) | 0x7F) + 1 == 2048` at the high end and `1` at the low end,
and `TurnoutMqttCommandDecoder` validates the untrusted MQTT address string
itself *before* constructing one, returning `nullopt` instead (see below).
Per this project's own principle — validate only at system boundaries, not
values that can't structurally occur — `TurnoutAddress` needs no runtime
check of its own.

**`DomainEvent`/`DomainCommand` as `std::variant`, not a base class:** adding a
future device type means adding one alternative to the variant, not growing
an inheritance hierarchy. Every consumer (`TurnoutMqttEncoder`, the eventual
second device type's encoder) dispatches via `std::visit`, so a missed case
anywhere is a compile error, not a silent runtime gap. This still satisfies
"never touching the router" — the variant's own definition lives in the
domain layer the routers don't see into; only the list of alternatives
grows when a device type is added.

## Ports

Reused unchanged from the commissioning sub-project: `LocoNetPort`
(send/receive), `Clock`.

```cpp
// ports/LocoNetMessageDecoder.h
class LocoNetMessageDecoder {
public:
    virtual ~LocoNetMessageDecoder() = default;
    virtual bool canDecode(uint8_t opcode) const = 0;
    virtual std::optional<DomainEvent> decode(const LocoNetMessage& message) = 0;
    virtual std::vector<DomainEvent> allKnownStates() const = 0;
};

// ports/MqttEventEncoder.h
class MqttEventEncoder {
public:
    virtual ~MqttEventEncoder() = default;
    virtual MqttMessage encode(const DomainEvent& event) const = 0;
};

// ports/MqttCommandDecoder.h
class MqttCommandDecoder {
public:
    virtual ~MqttCommandDecoder() = default;
    virtual bool canDecode(const std::string& deviceTypeSegment) const = 0;
    virtual std::optional<DomainCommand> decode(const std::string& address, const std::string& payload) const = 0;
};

// ports/LocoNetSendScheduler.h — see the layering note below
class LocoNetSendScheduler {
public:
    virtual ~LocoNetSendScheduler() = default;
    virtual void sendNow(const LocoNetMessage& message) = 0;
    virtual void sendAfter(const LocoNetMessage& message, unsigned long delayMilliseconds) = 0;
};

// ports/LocoNetEncoder.h
class LocoNetEncoder {
public:
    virtual ~LocoNetEncoder() = default;
    virtual void encode(const DomainCommand& command, LocoNetSendScheduler& scheduler) const = 0;
};

// ports/MqttPort.h — mirrors LocoNetPort's send+receive combination for its bus
class MqttPort {
public:
    virtual ~MqttPort() = default;
    virtual void publish(const MqttMessage& message) = 0;
    virtual std::optional<IncomingMqttMessage> receiveCommand() = 0;
};
```

**Deviations from the architecture doc's literal `LocoNetMessageDecoder`
signature, and why:** the doc's example returns `DomainEvent` unconditionally
from a `const decode()`. This sub-project's spec requires publishing `state`
"only when the value actually changes... compare against the last-known
position before publishing." That comparison must survive across calls, and
it is turnout-specific (an address → position map), so it can't live in the
generic router (which must stay device-agnostic) or in a stateless MQTT
encoder (which shouldn't need to know "no event" is possible). It belongs in
`TurnoutLocoNetDecoder` itself. That makes `decode()` non-`const` (it owns
`std::map<int, TurnoutPosition> lastKnownPosition_`) and its return type
`std::optional<DomainEvent>`, where `nullopt` means "decoded fine, nothing
changed, don't publish." `MqttCommandDecoder`/`MqttEventEncoder`/
`LocoNetEncoder` match the architecture doc's shapes exactly.

A second addition, `allKnownStates() const`, exists solely to support the
periodic full re-publish described under "MQTT contract" below — it returns
a `DomainEvent` for every address the decoder currently has state for
(empty for a freshly booted decoder that hasn't seen any traffic yet). Any
future read-only device type implements it the same way, trivially, from
whatever state map it already keeps for its own dedup logic.

## LocoNetSendScheduler port and its implementation

**Layering note:** an earlier draft of this design had `LocoNetEncoder`
(a port) take a concrete `PendingLocoNetSendScheduler` (an application
class) directly as a parameter — backwards from this project's stated rule
that ports never depend on application code. Fixed by extracting
`LocoNetSendScheduler` as its own port (shown above, alongside the other
four); `PendingLocoNetSendScheduler` is that port's one real
implementation, and a hand-written `FakeLocoNetSendScheduler` joins
`test/support/` so `TurnoutLocoNetEncoder`'s tests don't need to route
through real timing logic to check what it asked to be sent.

`PendingLocoNetSendScheduler` itself has no hardware dependency of its
own — only `LocoNetPort&` and `Clock&`, both already fake-able from the
commissioning sub-project. This is what lets the LocoNet write pipeline
issue a turnout's on-pulse immediately and its off-pulse after a delay
without ever calling a blocking `delay()`.

```cpp
// application/PendingLocoNetSendScheduler.h
class PendingLocoNetSendScheduler : public LocoNetSendScheduler {
public:
    PendingLocoNetSendScheduler(LocoNetPort& port, Clock& clock);
    void sendNow(const LocoNetMessage& message) override;
    void sendAfter(const LocoNetMessage& message, unsigned long delayMilliseconds) override;
    void update();  // called every loop() tick; sends anything now due
private:
    LocoNetPort& port_;
    Clock& clock_;
    std::vector<PendingLocoNetSend> pending_;
};
```

`TurnoutLocoNetEncoder::encode()` calls `scheduler.sendNow(onPulse)` then
`scheduler.sendAfter(offPulse, kOffPulseDelayMs)` against the
`LocoNetSendScheduler&` it's given — tested against
`FakeLocoNetSendScheduler` in isolation. `PendingLocoNetSendScheduler`'s
own timing behavior (does `sendAfter` actually wait, does `update()` fire
at the right moment) is tested separately, wired to the real
`FakeLocoNetPort` + `FakeClock` — a genuine object under test, not a mock
standing in for one.

`TurnoutLocoNetEncoder::encode()` calls `scheduler.sendNow(onPulse)` then
`scheduler.sendAfter(offPulse, kOffPulseDelayMs)`. Tested with the real
scheduler wired to `FakeLocoNetPort` + `FakeClock` — no new fake needed, and
this is a genuine object under test, not a mock standing in for one.

## Routers

Exactly as the architecture doc describes, and pure logic in both cases —
tested with 2-3 trivial fake decoders each, independent of turnout specifics:

- `LocoNetMessageRouter` — constructed with a
  `vector<pair<LocoNetMessageDecoder*, MqttEventEncoder*>>`, an `MqttPort&`,
  and a `Clock&` (constructor injection, wired once in the composition
  root). Given an incoming `LocoNetMessage`, finds the pair whose decoder's
  `canDecode` matches the opcode, decodes it (may yield nothing — see the
  dedup note above), and if it did, encodes and publishes it via
  `MqttPort::publish()`. Its `update()` (called every `loop()` tick) checks
  the injected `Clock` against `kStateRepublishIntervalMs = 30000` and, when
  due, calls `allKnownStates()` on every registered decoder and publishes
  each result through its paired encoder — the late-subscriber fix described
  under "MQTT contract" above.
- `MqttCommandRouter` — constructed with a `vector<MqttCommandDecoder*>`.
  Parses the incoming topic's device-type segment once, dispatches to the
  matching decoder, forwards the resulting command to the matching
  `LocoNetEncoder`.

## Turnout device type

```cpp
// adapters or domain (pure logic, no Arduino dependency; lives under a
// device-type-specific location, e.g. lib/Loco2MqttCore/src/turnout/)
class TurnoutLocoNetDecoder : public LocoNetMessageDecoder {
public:
    bool canDecode(uint8_t opcode) const override;  // OPC_SW_REQ, OPC_SW_REP
    std::optional<DomainEvent> decode(const LocoNetMessage&) override;
private:
    std::map<int, TurnoutPosition> lastKnownPosition_;
};

class TurnoutMqttEncoder : public MqttEventEncoder {
public:
    MqttMessage encode(const DomainEvent&) const override;
    // topic: "loconet/turnout/<address>/state", retained=true,
    // payload: "CLOSED" | "THROWN"
};

class TurnoutMqttCommandDecoder : public MqttCommandDecoder {
public:
    bool canDecode(const std::string& deviceTypeSegment) const override;  // == "turnout"
    std::optional<DomainCommand> decode(const std::string& address, const std::string& payload) const override;
    // returns nullopt for a malformed address (not an integer, or outside
    // 1..2048) or payload (not exactly "CLOSED"/"THROWN") right at this
    // boundary, so nothing downstream needs defensive checks; MqttCommandRouter
    // silently drops a nullopt result (no LocoNet traffic is sent)
};

class TurnoutLocoNetEncoder : public LocoNetEncoder {
public:
    void encode(const DomainCommand&, LocoNetSendScheduler&) const override;
    // sendNow(onPulse); sendAfter(offPulse, kOffPulseDelayMs)
};

// domain/LocoNetChecksum.h — pure helper, needed because the vendor
// library's TX path never computes one itself (see "Protocol bytes" below)
uint8_t computeLocoNetChecksum(const std::vector<uint8_t>& bytesBeforeChecksum);
```

### MQTT contract

| Topic | Retained (requested) | Payload |
|---|---|---|
| `loconet/turnout/<address>/state` | yes | `"CLOSED"` \| `"THROWN"` |
| `loconet/turnout/<address>/set` | no | `"CLOSED"` \| `"THROWN"` |

**QoS and retained-flag reality, confirmed against
[PicoMQTT's actual README](https://github.com/mlesniew/PicoMQTT):** its
broker mode "only supports MQTT QoS level 0, ignores will and retained
messages" — there is no QoS 1 support and the `retained` flag has no effect
at all in broker mode. `MqttMessage::retained()` is still set `true` for
the `state` topic (so the code documents intent and a future broker swap
gets it for free), but no reliability is derived from it today.

**Late-subscriber gap and the fix, per the user's decision:** because
retained delivery doesn't exist and PicoMQTT's `Server` exposes no
per-client subscribe/connect hook, a device that subscribes to
`loconet/turnout/<address>/state` after boot would otherwise learn nothing
until the next physical change. `LocoNetMessageRouter` closes this gap with
a periodic full re-publish: every `kStateRepublishIntervalMs = 30000`, it
asks every decoder for everything it currently knows
(`LocoNetMessageDecoder::allKnownStates()`, a new port method — see below)
and publishes each one through the matching encoder, in addition to
publishing immediately on every real change. A late subscriber is
guaranteed a correct value within 30 seconds of connecting.

### Protocol bytes — cited from JMRI, not guessed

`LocoNetESP32HB` (the vendor library already in use) is a raw byte
transport only; it defines no `OPC_SW_REQ`/`OPC_SW_REP` opcode or bit-field
constants, and its `lnWriteMsg`/`hybrid_write` path copies `lnData` verbatim
onto the wire with **no checksum computation of its own** — confirmed by
reading `.pio/libdeps/esp32dev/ESPLocoNetHybridESP32/src/IoTT_LocoNetHBESP32.cpp`
directly: the RX path calls `getXORCheck(...)` to *validate* incoming
frames, but the TX path (`lnWriteMsg` → `hybrid_write`) never calls it.
**`TurnoutLocoNetEncoder` must compute and append the checksum byte itself.**

Bit layout, cited from JMRI's `LnConstants.java`
(`java/src/jmri/jmrix/loconet/LnConstants.java`,
[JMRI/JMRI on GitHub](https://github.com/JMRI/JMRI/blob/master/java/src/jmri/jmrix/loconet/LnConstants.java)):

```
OPC_SW_REQ      = 0xB0
OPC_SW_REP      = 0xB1
OPC_SW_REQ_DIR  = 0x20   // in OPC_SW_REQ's SW2: set = Closed, clear = Thrown
OPC_SW_REQ_OUT  = 0x10   // in OPC_SW_REQ's SW2: set = output on, clear = output off
OPC_SW_REP_INPUTS = 0x40 // in OPC_SW_REP's SW2: set = sensor/input report, clear = switch/output report
OPC_SW_REP_CLOSED = 0x20 // in OPC_SW_REP's SW2, only meaningful when INPUTS is clear: set = Closed
```

**`OPC_SW_REQ` — 4 bytes: `[0xB0, SW1, SW2, CHECKSUM]`** (both the message
this bridge transmits, and the echo/other-throttle traffic it receives on
the same opcode):
- `SW1 = (address - 1) & 0x7F` — low 7 bits of the zero-based address.
- `SW2 = (((address - 1) >> 7) & 0x0F) | (position == Closed ? 0x20 : 0x00) | (outputOn ? 0x10 : 0x00)`
  — low nibble is the zero-based address's high bits (sufficient for this
  spec's 1..2048 range, which needs only bits 7-10); `0x20` is
  `OPC_SW_REQ_DIR`; `0x10` is `OPC_SW_REQ_OUT`.
- `CHECKSUM` — the byte `X` such that `0xB0 ^ SW1 ^ SW2 ^ X == 0xFF`
  (LocoNet's standard XOR checksum convention; matches the vendor library's
  own `getXORCheck` validation, which requires the XOR of all message bytes
  including the checksum to equal `0xFF`).
- The on-pulse is this message with `outputOn = true`; the off-pulse
  (sent `kOffPulseDelayMs` later via `PendingLocoNetSendScheduler`) is the
  same address and direction with `outputOn = false`.

**`OPC_SW_REP` — decode only, address/direction extraction (v1 handles only
the switch/output-report variant; a sensor/input report is decoded to
`nullopt` — not a gap to fill, matching the doc's explicit "not one to
build speculatively" note)**:
- If `SW2 & 0x40` (`OPC_SW_REP_INPUTS`) is set: this is a sensor report, not
  a commanded-position report — `decode()` returns `nullopt`.
- Otherwise: `address = (((SW2 & 0x0F) << 7) | (SW1 & 0x7F)) + 1`;
  `position = (SW2 & 0x20) ? Closed : Thrown`.

`TurnoutLocoNetDecoder::canDecode()` returns true for both `0xB0` and
`0xB1`; `decode()` applies `OPC_SW_REQ`'s address/direction extraction
(same formula as above, ignoring the `OPC_SW_REQ_OUT` on/off bit — both the
on-pulse and its later off-pulse describe the same commanded position) for
`0xB0`, and the `OPC_SW_REP` rule above for `0xB1`.

### Turnout pulse timing

`kOffPulseDelayMs = 250` as a starting value for the on-pulse → off-pulse
gap. This is a device-timing choice under our control, not a wire-protocol
fact — flagged for verification against real turnout hardware the same way
`kLocoNetTxPin`'s boot-safety is flagged in the existing README, adjustable
if a real decoder needs longer.

## WiFi connection + PicoMQTT broker

New adapters, build-check-only (same convention as the existing ESP32
hardware shims — no native test, verified by `pio run -e esp32dev`):

- `EspWifiPort` — non-blocking. Its `update()` checks `WiFi.status()` and
  (re)issues `WiFi.begin(ssid, password)` on a backoff (e.g. every 5s)
  whenever disconnected. Never blocks `loop()`. Per the user's decision,
  never falls back into the wireless-setup AP on its own — commissioning
  stays a deliberate, BOOT-button-triggered action.
- `PicoMqttPort implements MqttPort` — wraps a `PicoMQTT::Server` instance
  running on-device, confirmed against
  [PicoMQTT's README](https://github.com/mlesniew/PicoMQTT):
  `PicoMQTT::Server mqtt;` constructed with no arguments, `mqtt.begin()`
  called once (after WiFi is up), `mqtt.publish(topic, payload)` for
  `MqttPort::publish()`, `mqtt.subscribe("loconet/+/+/set", callback)`
  registered once at construction to feed a queue that `receiveCommand()`
  pulls from (matching this codebase's existing pull-based port style, e.g.
  `LocoNetPort::receive()`), and `mqtt.loop()` — called from `update()`,
  mirroring the existing `locoNetPort->update()` pattern in `src/main.cpp`
  — to pump client connections and message routing.
- New pinned dependency in `platformio.ini`: PicoMQTT, pinned to a specific
  commit (same convention as `LocoNetESP32HB`/`ArduinoJson`, since neither
  repo is guaranteed to have tagged releases).

### BootMode boundary

The MQTT bridge (WiFi, PicoMQTT, both routers, the turnout decoders/
encoders, the send scheduler) is constructed only when `BootMode::Normal` —
i.e., commissioning is already complete. In `BootMode::NeedsCommissioning`,
behavior is unchanged from today: LocoNet message logging plus serial
commissioning only, no WiFi/MQTT attempted, since there's no saved SSID to
connect with yet. `BootMode::WirelessSetup` is unaffected — it still
excludes the LocoNet/MQTT slice entirely, as it already does today.

## Testing strategy

Two-tier, matching the commissioning sub-project's precedent:

- **Native-tested** (fakes only, no mocking framework): `TurnoutAddress`,
  `TurnoutStateChanged`/`SetTurnoutPosition`, `computeLocoNetChecksum`,
  `LocoNetMessageRouter` (dispatch, dedup pass-through, and the periodic
  `allKnownStates()` re-publish path, all with trivial fake decoders/
  encoders plus `FakeMqttPort`+`FakeClock`), `MqttCommandRouter`,
  `PendingLocoNetSendScheduler` (with `FakeLocoNetPort`+`FakeClock`),
  `TurnoutLocoNetDecoder` (including the OPC_SW_REP sensor-report-returns-
  nullopt case and `allKnownStates()`), `TurnoutMqttEncoder`,
  `TurnoutMqttCommandDecoder`, `TurnoutLocoNetEncoder` (against
  `FakeLocoNetSendScheduler`, asserting the exact on-pulse/off-pulse bytes
  including the computed checksum). `FakeMqttPort` and
  `FakeLocoNetSendScheduler` join `test/support/` alongside the existing
  fakes.
- **Build-check only** (no native test, `pio run -e esp32dev`):
  `EspWifiPort`, `PicoMqttPort` — genuinely hardware-bound, matching
  `LocoNetEsp32Port`'s existing precedent.

## Resolved during design (previously open items)

- **JMRI byte layout** — cited above from `LnConstants.java`; see "Protocol
  bytes."
- **PicoMQTT QoS/retained-message support** — confirmed via its README:
  broker mode is QoS 0 only and ignores retained entirely. Addressed with
  the periodic full re-publish described under "MQTT contract."
- **PicoMQTT C++ API shape** — confirmed via its README; see "WiFi
  connection + PicoMQTT broker" above.
