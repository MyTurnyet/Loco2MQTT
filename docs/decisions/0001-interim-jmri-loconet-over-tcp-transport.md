# ADR 0001: Interim JMRI LocoNet-over-TCP Transport

## Status

Accepted. Implemented on branch `interim/jmri-transport`; on-hardware
validation (build order step 6) still pending.

**Partially superseded (2026-09-10):** this ADR's "no persistent JMRI
connection settings" scope decision no longer holds — see "Addendum
(2026-09-10)" below for what changed and why. Everything else on this page
(the transport design itself, the protocol findings, the revert plan) is
unaffected.

## Context

Loco2MQTT's electrical LocoNet interface (6N137 opto RX, 2N3904
open-collector TX, breadboard-built) is not yet reading reliably. All
router, decoder, and MQTT-bridging logic is fully covered by native tests
and had never run against real LocoNet traffic — every commit to date was
native-test-verified plus a build-only check against `esp32dev`. The
hardware bring-up is correctly a separate, ongoing effort; it should not
continue to block validating the rest of the system against real bus
activity.

JMRI already has a working, reliable connection to the DR5000 — the same
connection the prior Jython-based bridge relied on. JMRI's "Start LocoNet
Server" feature exposes that connection over a plain-ASCII, line-based TCP
protocol (LocoNetOverTcp, default port 1234): clients send `SEND <hex
bytes>` to inject a message, and receive inbound LocoNet traffic back as
ASCII lines over the same socket.

`LocoNetPort` — the existing seam between the router/application layer and
the physical bus — is already about as narrow as it could be:

```cpp
class LocoNetPort
{
public:
    virtual ~LocoNetPort() = default;
    virtual std::optional<LocoNetMessage> receive() = 0;
    virtual void send(const LocoNetMessage& message) = 0;
};
```

A second implementation of this port, backed by JMRI's TCP feed instead of
the electrical bus, lets every other layer of the system be validated
against real, live LocoNet traffic today, independent of the hardware
bring-up.

## Decision

Build a JMRI-backed `LocoNetPort` implementation that runs on the ESP32,
over WiFi, in place of `LocoNetEsp32Port`. This is deliberately narrower
than a native/PC target: it reuses the WiFi connection and PicoMQTT broker
that already exist in the `esp32dev` firmware, so the only thing being
swapped is the transceiver. Everything downstream — router, decoders,
encoders, MQTT bridging — runs exactly as it will in production.

This is explicitly a gap-filler, not a permanent architecture change. It
exists to unblock validation while the electrical LocoNet interface is
debugged separately, and is expected to be removed from the runtime path
(though not necessarily deleted — see "Revert plan") once that hardware
work is done. Scope is kept deliberately minimal on that basis: no new
commissioning flow, no runtime-configurable transport selection, no
persistent JMRI connection settings.

## Consequences

- Router/decoder/MQTT-bridge logic gets validated against real bus traffic
  without waiting on the opto/transistor circuit.
- The firmware gains a second, permanently-available `LocoNetPort`
  implementation — useful later for bench testing without a physical bus,
  even after the hardware path is working.
- JMRI must be running and reachable over WiFi for this transport to work.
  That's an accepted, temporary dependency, not a new permanent one — it
  matches what the prior Jython-based approach already required.
- **Line framing is not `\n`-based.** A netcat capture against a real,
  running JMRI 5.2 instance (2026-09-10, see "Protocol findings" below)
  showed every line terminated with a bare `\r` — no `\n`, not even `\r\n`.
  The existing `LineAssembler` (shared with USB-serial commissioning via
  `EspUartPort`) only recognizes `\n`, so it could not be reused unmodified
  without risking a regression on that shared, already-tested path.
  `WiFiClientLineStream` therefore has its own small, self-contained,
  `\r`-terminated buffering loop rather than reusing `LineAssembler` —
  a deliberate, explicitly-confirmed tradeoff (a few duplicated lines of
  framing logic, in exchange for zero risk to the commissioning path).

## Protocol findings (empirical, 2026-09-10)

Confirmed against a real, running JMRI 5.2 instance at `192.168.1.13:1234`
via netcat, per this ADR's own build order (step 1) — not assumed from the
protocol reference doc alone:

```
RECV: b'VERSION JMRI Server 5.2+R760b98537f\r'
(SEND BB 01 00 45 sent to the server)
RECV: b'RECEIVE BB 01 00 45\r'
RECV: b'SENT OK\r'
RECV: b'RECEIVE E7 0E 01 33 0F 00 00 07 08 4E 00 35 44 1B\r'
```

- Greeting: `VERSION <free text>`, terminated `\r`.
- Outbound: `SEND <space-separated uppercase hex bytes>` — matches the
  generic LocoNetOverTcp protocol doc exactly (confirmed against the
  documented worked example `SEND A0 2F 00 70`).
- Inbound data: `RECEIVE <space-separated uppercase hex bytes>`,
  terminated `\r` — used both for genuine bus traffic and to echo back a
  message this adapter itself sent.
- Send confirmation: `SENT OK` (and, per the protocol doc, `SENT ERROR
  <comment>` on failure) — a separate line, sent after the `RECEIVE` echo,
  not carrying any LocoNet payload itself.
- All non-`RECEIVE` lines (`VERSION`, `SENT OK`/`SENT ERROR`) are treated
  as non-data by `LocoNetOverTcpCodec::decodeLine()` and ignored.

## Alternatives considered

- **Native/PC target, real MQTT broker, no ESP32 involved.** Faster to
  iterate on, but validates less: it wouldn't exercise the real
  WiFi/PicoMQTT path, which is exactly what still needed confirming on real
  hardware. Rejected for this pass since the ESP32 path is what the
  router/decoder work had never actually run against.
- **Fix the hardware first, skip this entirely.** Keeps the two problems
  (hardware bring-up, software validation) coupled with no way to make
  progress on one while blocked on the other. Rejected as the whole reason
  this ADR exists.
- **Reuse `LineAssembler` for line framing** (generalize it to accept a
  configurable terminator). More DRY, but `LineAssembler` is shared with
  the already-tested, already-shipped USB-serial commissioning path;
  changing its terminator semantics would need its own new tests and
  careful handling of CRLF/LFCR inputs there so they don't start
  double-firing. Rejected in favor of a small, self-contained,
  `#ifdef ARDUINO`-guarded loop local to `WiFiClientLineStream` — the same
  tier (build-check only, not natively tested) as `LocoNetEsp32Port`
  already occupies, so the risk of getting the framing detail wrong is
  already confined to a place with no native test coverage regardless.

## Revert plan

Work happens on branch `interim/jmri-transport`, not `main`. Reverting is
then "don't merge this branch" rather than unpicking conditional logic. If
the electrical interface starts working before this branch is merged, it
can simply be set aside. If it's already merged when the hardware is
fixed, revert the one composition-root change in `main.cpp` back to
`LocoNetEsp32Port` (every line touched there is marked `INTERIM`) and leave
the JMRI adapter's files in place as a standing dev/debug tool — they cost
nothing to keep and don't interfere with the hardware path once unwired
from `main.cpp`.

## Implementation status

Build order, from the accompanying spec (below), and what's done:

1. ✅ Protocol verification spike — see "Protocol findings" above.
2. ✅ `LocoNetOverTcpCodec` — `encodeSend()` TDD'd against the documented
   `SEND` syntax; `decodeLine()` TDD'd against the real captured lines
   above.
3. ✅ `LocoNetOverTcpPort` — TDD'd against `FakeLineStream`.
4. ✅ `WiFiClientLineStream` — written, `#ifdef ARDUINO`-guarded. Compiles
   to an empty translation unit under the native build (confirmed). **Not**
   build-checked against `esp32dev` — see note below.
5. ✅ Composition root swap — `src/main.cpp` on `interim/jmri-transport`.
6. ⬜ On-hardware validation — flash, confirm connection to JMRI over
   WiFi, confirm a real turnout state change reaches MQTT, confirm an MQTT
   command reaches the layout. Not yet done.

**Toolchain note:** all native TDD work (steps 2–3) was done and verified
in a sandbox where PlatformIO's own package registry
(`api.registry.platformio.org`) is blocked by network policy, so `pio test
-e native` and `pio run -e esp32dev` couldn't be run directly. Native tests
were instead run by building the vendored Catch2 v3 sources into a static
library with `g++` directly and linking each test binary against it by
hand — functionally equivalent to what `pio test -e native` does, since
that environment already just invokes the host's own compiler. This
doesn't extend to `esp32dev`, which needs the actual `espressif32`
platform package: **`WiFiClientLineStream` and the `main.cpp` composition
root change have not been build-checked, only reviewed.** Run `pio run -e
esp32dev` before flashing.

---

# Spec: JMRI LocoNet-over-TCP Adapter

## Engineering principles (carried over, non-negotiable)

- TDD, no mocking frameworks — real objects or hand-written fakes only.
- Methods ≤ 8 lines, cognitive complexity < 4.
- Constructor-based dependency injection; no statics, no singletons.
- Composition over inheritance.
- Immutable value objects for all domain data.
- Ask, don't tell.
- Arduino/hardware dependencies confined to the thinnest possible shim,
  `#ifdef ARDUINO`-guarded, not natively tested. Everything else — even new
  code — gets full native TDD coverage.

## Scope

In scope: a JMRI-backed `LocoNetPort` implementation, wired in for the
`esp32dev` build on the interim branch, validated end-to-end against a real
running JMRI instance and the DR5000.

Out of scope: ~~any persistent configuration UI for the JMRI host/port
(hardcoded constants are fine, matching how `kLocoNetRxPin` etc. are
handled today)~~ — **superseded, see "Addendum (2026-09-10)" below**; any
change to the electrical LocoNet interface work, which continues
independently; any change to `LocoNetPort` itself, the router, or any
decoder/encoder.

## Architecture

Because the transport target is now a TCP socket rather than a UART, the
Arduino-specific surface is pushed one layer further down than
`LocoNetEsp32Port` — nearly everything about this adapter is natively
tested.

```
ports/LineStream.h                          (new port, no Arduino dependency)
adapters/LocoNetOverTcpCodec.{h,cpp}        (pure logic, natively tested)
adapters/LocoNetOverTcpPort.{h,cpp}         (implements LocoNetPort, natively tested)
adapters/WiFiClientLineStream.{h,cpp}       (implements LineStream, #ifdef ARDUINO, thin, not natively tested)
test/support/FakeLineStream.h               (hand-written fake)
```

`LineStream` is shaped like the existing `UartPort` (`readLine()`/
`writeLine()`) deliberately, but kept as its own port: it adds
`isConnected()`, a concept a socket needs and USB serial in this codebase
doesn't.

`LocoNetOverTcpPort` composes a `LineStream&` and owns a
`LocoNetOverTcpCodec` by value (there's exactly one wire format, so nothing
to substitute — unlike `LocoNetPort`, which has multiple real/fake
implementations). It translates `LocoNetPort::send()`/`receive()` to
`LineStream::writeLine()`/`readLine()` plus the codec, and transparently
skips non-data lines (`VERSION`, `SENT OK`/`ERROR`) inside `receive()` so a
buffered real message is never delayed behind one. It has no Arduino
dependency itself, so it's tested natively against `FakeLineStream` — only
`WiFiClientLineStream` (the actual socket wrapper) is Arduino-guarded and
untested.

## Build order

See "Implementation status" above for what's done against each step.

1. Protocol verification spike (no code) — done, see "Protocol findings".
2. TDD `LocoNetOverTcpCodec` — done.
3. TDD `LocoNetOverTcpPort` against `FakeLineStream` — done.
4. Write `WiFiClientLineStream` — done, not yet build-checked against
   `esp32dev`.
5. Composition root swap on `interim/jmri-transport` — done.
6. On-hardware validation — pending.

---

## Addendum (2026-09-10): JMRI host/port made runtime-configurable

Supersedes this ADR's original "no persistent JMRI connection settings"
scope decision (see the struck-through line in "Scope" above and "Scope is
kept deliberately minimal..." in "Decision").

**What changed:** `LocoNetAdapterConfig` gained two fields — `jmriHost`
(`std::string`) and `jmriPort` (`uint16_t`) — alongside the existing
`wifiSsid`/`wifiPassword`. They're commissioned through the same two front
doors WiFi credentials already use: bench-serial (`set-jmri-host <host>`,
`set-jmri-port <port>`, both reflected by `show`) and the captive-portal web
form (`JMRI Host`/`JMRI Port` fields). Both persist through the same
`ConfigStore`/`NvsConfigStore` the WiFi fields already used — two new NVS
keys (`jmri_host`, `jmri_port`), no change to the port interface itself.
`isComplete()` now requires all four fields, so a board commissioned with
WiFi alone (no JMRI host/port) sits in `BootMode::NeedsCommissioning` until
both are set, the same way an incomplete WiFi commissioning already
behaved — this means any board already commissioned before this change
needs a one-time recommissioning trip to add the JMRI host/port before it
will boot to `Normal` again. `src/main.cpp` now reads the commissioned
host/port at boot instead of the `kJmriHost`/`kJmriPort` constants this ADR
originally scoped for; those constants are removed.

**Why:** the original scope assumed this transport would stay pointed at
one JMRI instance at one fixed address for its whole (short) interim
lifetime, so a hardcoded constant was the simplest thing that could work.
In practice, pointing it at a different JMRI instance — a different bench
setup, a machine that moved — meant editing `main.cpp` and reflashing,
which is exactly the friction the WiFi-commissioning mechanism already
exists to avoid elsewhere in this firmware. The Revert plan's own framing
of this adapter as a "standing dev/debug tool" worth keeping even after the
electrical interface works was the signal that it was worth the small
amount of extra commissioning-surface area now, rather than repeating this
exercise later.

**What didn't change:** this remains the interim transport, not a
permanent architectural fixture — the Revert plan above is unaffected by
this addendum. The new code follows the same discipline as everything else
in this codebase: TDD, hand-written fakes only, ≤8-line methods. The port
number validation is its own small pure function (`NetworkPortParser`,
`domain/NetworkPortParser.h`/`.cpp`), shared by both the bench-serial
parser and the web form so malformed input is rejected identically at
either front door — natively tested in `test_network_port_parser`.
