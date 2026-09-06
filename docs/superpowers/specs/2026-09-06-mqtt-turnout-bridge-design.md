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
// domain/TurnoutAddress.h — immutable, validated
class TurnoutAddress {
public:
    explicit TurnoutAddress(int address);  // valid range: 1..2048 (LocoNet's 11-bit address space)
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
    PendingLocoNetSend(LocoNetMessage message, unsigned long dueAtMillis);
    const LocoNetMessage& message() const;
    unsigned long dueAtMillis() const;
private:
    LocoNetMessage message_;
    unsigned long dueAtMillis_;
};
```

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
    virtual DomainCommand decode(const std::string& address, const std::string& payload) const = 0;
};

// ports/LocoNetEncoder.h
class LocoNetEncoder {
public:
    virtual ~LocoNetEncoder() = default;
    virtual void encode(const DomainCommand& command, PendingLocoNetSendScheduler& scheduler) const = 0;
};

// ports/MqttPort.h — mirrors LocoNetPort's send+receive combination for its bus
class MqttPort {
public:
    virtual ~MqttPort() = default;
    virtual void publish(const MqttMessage& message) = 0;
    virtual std::optional<IncomingMqttMessage> receiveCommand() = 0;
};
```

**Deviation from the architecture doc's literal `LocoNetMessageDecoder`
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

## PendingLocoNetSendScheduler

A concrete class, not a port — it has no hardware dependency of its own,
only `LocoNetPort&` and `Clock&`, both already fake-able from the
commissioning sub-project. This is what lets the LocoNet write pipeline
issue a turnout's on-pulse immediately and its off-pulse after a delay
without ever calling a blocking `delay()`.

```cpp
// application/PendingLocoNetSendScheduler.h
class PendingLocoNetSendScheduler {
public:
    PendingLocoNetSendScheduler(LocoNetPort& port, Clock& clock);
    void sendNow(const LocoNetMessage& message);
    void sendAfter(const LocoNetMessage& message, unsigned long delayMillis);
    void update();  // called every loop() tick; sends anything now due
private:
    LocoNetPort& port_;
    Clock& clock_;
    std::vector<PendingLocoNetSend> pending_;
};
```

`TurnoutLocoNetEncoder::encode()` calls `scheduler.sendNow(onPulse)` then
`scheduler.sendAfter(offPulse, kOffPulseDelayMs)`. Tested with the real
scheduler wired to `FakeLocoNetPort` + `FakeClock` — no new fake needed, and
this is a genuine object under test, not a mock standing in for one.

## Routers

Exactly as the architecture doc describes, and pure logic in both cases —
tested with 2-3 trivial fake decoders each, independent of turnout specifics:

- `LocoNetMessageRouter` — constructed with a `vector<LocoNetMessageDecoder*>`
  (constructor injection, wired once in the composition root). Given an
  incoming `LocoNetMessage`, finds the decoder whose `canDecode` matches the
  opcode, decodes it (may yield nothing — see the dedup note above), forwards
  any resulting event to the matching `MqttEventEncoder`, publishes via
  `MqttPort::publish()`.
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
    DomainCommand decode(const std::string& address, const std::string& payload) const override;
    // rejects malformed address or payload (not exactly "CLOSED"/"THROWN")
    // right at this boundary, so nothing downstream needs defensive checks
};

class TurnoutLocoNetEncoder : public LocoNetEncoder {
public:
    void encode(const DomainCommand&, PendingLocoNetSendScheduler&) const override;
    // sendNow(onPulse); sendAfter(offPulse, kOffPulseDelayMs)
};
```

### MQTT contract

| Topic | Retained | Payload |
|---|---|---|
| `loconet/turnout/<address>/state` | yes | `"CLOSED"` \| `"THROWN"` |
| `loconet/turnout/<address>/set` | no | `"CLOSED"` \| `"THROWN"` |

QoS: use QoS 1 if PicoMQTT supports it without extra work; QoS 0 is
acceptable if not. Retained state plus reconnect-triggered republish is
enough reliability for this use case — this sub-project does not block on
QoS 1 support.

### Protocol bytes — resolved during plan-writing, not guessed here

`LocoNetESP32HB` (the vendor library already in use) is a raw byte
transport only; it defines no `OPC_SW_REQ`/`OPC_SW_REP` opcode or bit-field
constants. The exact address-bit-split, direction-bit, and on/off-bit
polarity must come from an authoritative source. Per the user's direction,
the implementation plan will cite JMRI's own open-source LocoNet decoding
logic for the exact byte layout used in `TurnoutLocoNetDecoder`/
`TurnoutLocoNetEncoder`'s test cases and implementation — fetched and cited
by file/commit during plan-writing, never assumed from memory, and never
left as a placeholder in the finished plan.

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
  running on-device. Internally subscribes itself to `loconet/+/+/set` and
  queues incoming messages for pull-based `receiveCommand()`. Its `update()`
  pumps PicoMQTT's own loop, mirroring the existing
  `locoNetPort->update()` pattern in `src/main.cpp`.
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
  `TurnoutStateChanged`/`SetTurnoutPosition`, `LocoNetMessageRouter`,
  `MqttCommandRouter`, `PendingLocoNetSendScheduler` (with
  `FakeLocoNetPort`+`FakeClock`), `TurnoutLocoNetDecoder`,
  `TurnoutMqttEncoder`, `TurnoutMqttCommandDecoder`, `TurnoutLocoNetEncoder`
  (with the real scheduler over fakes). A new `FakeMqttPort` joins
  `test/support/` alongside the existing fakes.
- **Build-check only** (no native test, `pio run -e esp32dev`):
  `EspWifiPort`, `PicoMqttPort` — genuinely hardware-bound, matching
  `LocoNetEsp32Port`'s existing precedent.

## Open items for plan-writing

- Cite JMRI's exact `OPC_SW_REQ`/`OPC_SW_REP` byte layout (address split,
  direction bit, on/off bit) before writing any turnout decoder/encoder
  test cases.
- Confirm PicoMQTT's actual QoS support at implementation time; fall back to
  QoS 0 silently if QoS 1 isn't available, per the scope decision above.
- Confirm PicoMQTT's C++ API shape for on-device subscribe/publish (exact
  method names/signatures) before writing `PicoMqttPort`.
