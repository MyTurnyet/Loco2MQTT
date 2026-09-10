# Loco2MQTT

Firmware for an ESP32-based LocoNet adapter.

**Current status: Phase 1 with MQTT bridge, on an interim transport.** This
firmware connects to WiFi using stored commissioning credentials, runs an
on-device MQTT broker (PicoMQTT), and bridges turnout state bidirectionally
— sending LocoNet turnout changes to MQTT and accepting MQTT commands to
drive LocoNet turnout operations. MQTT bridging for sensor, transponder,
and other device types remains future work.

**The electrical LocoNet interface is temporarily swapped out.** The
breadboard opto/transistor interface (see
[`docs/breadboard-build-guide.md`](docs/breadboard-build-guide.md)) is not
yet reading reliably, so `src/main.cpp` currently talks to LocoNet over
WiFi via JMRI's LocoNetOverTcp server instead of the RX/TX pins — see
[`docs/decisions/0001-interim-jmri-loconet-over-tcp-transport.md`](docs/decisions/0001-interim-jmri-loconet-over-tcp-transport.md)
for why, and the revert plan once the electrical interface is working.

Built with Test-Driven Development and Hexagonal Architecture — domain and
application logic is tested natively on a desktop, independent of the ESP32
hardware and independent of any physical LocoNet bus.

## What it does today

- Receives LocoNet messages relayed over WiFi from JMRI's LocoNetOverTcp
  server (the interim transport — see "Current status" above), and
  transparently ignores JMRI's own non-data protocol lines (`VERSION`,
  `SENT OK`/`SENT ERROR`).
- Can still receive over a breadboard-built RX interface via
  `LocoNetEsp32Port`, which validates every frame against the vendor
  library's own collision/checksum/incomplete-frame flags before
  processing it — this code path is not currently wired into
  `src/main.cpp` (see "Current status" above).
- Connects to WiFi using stored commissioning credentials (see "WiFi
  commissioning" below).
- Runs PicoMQTT as an on-device broker and publishes/subscribes to turnout
  bridge topics.
- Bridges turnout state bidirectionally: LocoNet turnout change commands are
  published to MQTT topics, and MQTT messages are translated back into LocoNet
  commands sent to the bus.
- Can transmit a `LocoNetMessage` onto the bus (`LocoNetPort::send()` is
  implemented and hardware-build-checked), driven by the MQTT bridge component.
- Flashes an onboard/external activity LED for 40ms every time a LocoNet
  message is received, in every boot mode — see "Activity LED" below.

## What it doesn't do yet

- No MQTT bridging for sensors, transponders, or other device types beyond
  turnout — see "Known limitations" below.
- No PCB — the electrical interface, once re-enabled, runs on a breadboard
  interface only (a PCB phase is noted as future work in the build guide).

## Hardware requirements

On the current interim transport (see "Current status" above), this
firmware needs:

- An ESP32 dev board. This project has been built and tested against an
  ELEGOO ESP-WROOM-32 module; other ESP32 boards should work but haven't
  been verified.
- A running JMRI instance with "Start LocoNet Server" enabled (the
  LocoNetOverTcp feature, default port `1234`), reachable over WiFi from
  the ESP32, connected to a real LocoNet bus (e.g. a Digitrax DR5000).

The breadboard electrical interface is not required to run today's
firmware, but is still the eventual target:

- The breadboard opto/transistor interface described in
  [`docs/breadboard-build-guide.md`](docs/breadboard-build-guide.md) — a
  6N137 optocoupler RX stage, a 2N3904 open-collector TX driver, and an RJ12
  breakout for the LocoNet connector. That guide has the full bill of
  materials, breadboard layout, and a step-by-step bring-up procedure with
  bench tests at each stage.
- Access to a LocoNet bus to test against — a Digitrax DR5000 (or similar
  command station) with a free LocoNet port, or a second known-good LocoNet
  device.
- Recommended but not required: a USB logic analyzer or oscilloscope.
  LocoNet runs at 16.66 kbps; when something's wrong, seeing the actual
  signal edges is much faster than guessing.

**This firmware has not yet been flashed to real hardware or tested against
a live LocoNet bus as part of its own development** — every commit so far
has been verified by native unit tests (no hardware) plus a build-only check
against the `esp32dev` target (compiles and links, never flashed). Flashing
it and doing the live bus smoke test below is the first thing to do with a
real board.

## Software requirements

- [PlatformIO](https://platformio.org/) (CLI or the VS Code extension)
- A C++17 host compiler, for the native test suite (on Windows, a MinGW
  toolchain — see the troubleshooting note below)
- No other manual dependency installation — `platformio.ini` pins the two
  third-party libraries this project needs (see below) to specific commits,
  and PlatformIO fetches them automatically on first build

## Quick start

### 1. Run the native tests (no hardware needed)

```bash
pio test -e native
```

This compiles and runs every domain/application/port test against
hand-written fakes — no ESP32, no LocoNet bus, no serial port required. All
39 suites should pass. This is the fast feedback loop for any code change;
run it before touching real hardware.

### 2. Build the firmware

```bash
pio run -e esp32dev
```

This compiles the real firmware against the ESP32/Arduino toolchain and the
vendor LocoNet library. It does **not** require a board to be plugged in —
it's a pure build-and-link check.

### 3. Point it at JMRI

On the current interim transport (see "Current status" above), there's no
hardware to wire yet — instead, start JMRI's "Start LocoNet Server" (from
JMRI's main window, under Debug or LocoNet Tools depending on version),
confirm it's listening on port `1234`, and update `kJmriHost`/`kJmriPort`
in `src/main.cpp` to match your JMRI machine's address (see "Configuration
reference" below). Once the electrical interface is re-enabled (see the
ADR's revert plan), this step becomes wiring
[`docs/breadboard-build-guide.md`](docs/breadboard-build-guide.md) instead.

### 4. Flash it

```bash
pio run -e esp32dev --target upload
```

### 5. Watch it work

```bash
pio device monitor
```

(115200 baud.) With JMRI's LocoNet server running and the ESP32 on the same
WiFi network, you should see hex-formatted lines for bus traffic —
heartbeats, throttle activity, anything else already on the bus, relayed
through JMRI. If you send a message from another LocoNet device, it should
appear as a new line. See Troubleshooting if you see nothing, or garbage.

To run a single native test file instead of the whole suite:

```bash
pio test -e native -f test_<name>
```

## Configuration reference

Everything hardware-specific lives in one of two places:

| Setting | Where | Value | Why |
|---|---|---|---|
| JMRI host | `src/main.cpp`, `kJmriHost` | `"192.168.1.13"` | The interim LocoNet transport (see "Current status" above) — the address of a JMRI instance running "Start LocoNet Server". Update if that machine's address changes; not runtime-configurable, per the ADR's deliberately minimal scope. |
| JMRI port | `src/main.cpp`, `kJmriPort` | `1234` | JMRI's LocoNetOverTcp server default port. |
| RX pin *(currently inert — see "Current status")* | `src/main.cpp`, `kLocoNetRxPin` | `16` (UART2 RX) | Must be a hardware UART-capable pin — LocoNet's 16.66kbps timing needs the real UART, not a bit-banged read. Unused while the interim JMRI transport is wired in. |
| TX pin *(currently inert — see "Current status")* | `src/main.cpp`, `kLocoNetTxPin` | `17` | Any free GPIO (TX is bit-level timed in software), but **must be boot-safe** — see the warning below. Unused while the interim JMRI transport is wired in. |
| Activity LED pin | `src/main.cpp`, `kActivityLedPin` | `2` | Flashes on every received LocoNet message. GPIO2 is the onboard LED on most ESP32-WROOM-32 DevKit boards; confirm against your specific board, or wire an external LED + resistor to GND on any free GPIO and change this constant. |
| Activity LED flash duration | `src/main.cpp`, `kActivityFlashDurationMs` | `40` (ms) | How long the LED stays lit per flash. A message arriving before the previous flash ends restarts the window rather than queuing a second pulse. |
| `InverseLogic` | `lib/Loco2MqttCore/src/adapters/LocoNetEsp32Port.cpp`, `LocoNetEsp32Port`'s constructor | hardcoded `true` | This breadboard circuit's 6N137 opto output is inverted; not a runtime option, since it's a property of the physical circuit, not a config choice. |
| Serial baud rate | `src/main.cpp`, `kSerialBaudRate` | `115200` | Console/logging baud rate — unrelated to LocoNet's own 16.66kbps bus speed. |
| Receive queue cap | `lib/Loco2MqttCore/src/adapters/LocoNetEsp32Port.cpp`, `kMaxPendingMessages` | `32` | Bounds worst-case heap growth if messages arrive faster than the main loop drains them; oldest messages are dropped first once full. |
| Turnout on-pulse → off-pulse delay | `lib/Loco2MqttCore/src/turnout/TurnoutLocoNetEncoder.h`, `kOffPulseDelayMs` | `250` (ms) | How long the LocoNet command holds the turnout output "on" before releasing it. This is a device-timing choice, not a wire-protocol fact — if your turnout motor/decoder needs longer to complete its throw, increase this. |
| State re-publish interval | `lib/Loco2MqttCore/src/application/LocoNetMessageRouter.h`, `kStateRepublishIntervalMs` | `30000` (ms) | How often every known turnout's current state is re-published to MQTT, so a client that subscribes late still learns the current state quickly (works around PicoMQTT's broker not honoring the MQTT retained flag). |
| WiFi retry interval | `lib/Loco2MqttCore/src/adapters/EspWifiPort.h`, `kRetryIntervalMs` | `5000` (ms) | How often the firmware retries connecting to WiFi if it isn't currently connected. Retries forever in the background; never falls back into wireless setup mode on its own. |
| MQTT command queue cap | `lib/Loco2MqttCore/src/adapters/PicoMqttPort.h`, `kMaxPendingCommands` | `32` | Bounds worst-case heap growth from MQTT clients publishing commands faster than they're drained; oldest commands are dropped first once full. |
| mDNS hostname | `src/main.cpp`, `kMdnsHostname` | `"loco2mqtt"` | The board answers as `<value>.local` once WiFi connects. Fixed rather than user-configurable — revisit only if a second bridge needs to coexist on the same network. |
| mDNS retry interval | `lib/Loco2MqttCore/src/adapters/EspMdnsPort.h`, `kRetryIntervalMs` | `5000` (ms) | How often the firmware retries `MDNS.begin()` after a failed attempt (e.g. transient heap exhaustion). Matches `EspWifiPort`'s own retry-interval pattern. |

**TX pin boot safety — read this before picking a different pin.** A GPIO
that is ever observed high across an ESP32 reset will momentarily turn on
the 2N3904 driver and pull the *entire shared LocoNet bus* low, disrupting
every other device on the layout, not just this adapter. GPIO17 (the
default) is not one of the ESP32's documented strapping pins (GPIO0, 2, 5,
12, 15) and is not driven by the ROM bootloader, so it should be safe — but
**confirm against your specific board's datasheet, and verify with a scope
or meter through a reset cycle before connecting to a live bus.** This
matters more than it sounds like it should: it's the one mistake here that
can affect equipment you don't own.

If you swap to an ESP32-WROVER module rather than a plain WROOM board, note
that GPIO16/17 are wired internally to that module's PSRAM and are not
available as GPIO at all — pick different pins.

## WiFi commissioning

The firmware needs a WiFi SSID and password before it can eventually bring
up MQTT — this is never hardcoded. There are two ways to set it:

**Bench-serial**, over the same USB connection used for `pio device
monitor` (115200 baud), whenever the device boots with no config saved yet:

```
set-ssid MyHomeWifi
set-password hunter2
save
```

`show` echoes the current in-progress SSID (never the password). Nothing
is written to flash until `save`.

**Wireless setup**, for a board already mounted on the layout: hold the
BOOT button for 3 seconds. The device reboots into an open WiFi access
point named `Loco2MQTT-Setup` with no password; connecting to it and
visiting any URL should open a setup page automatically (a captive
portal). Submitting the form saves the config and reboots back to normal
operation.

## MQTT turnout bridge

In normal boot mode, the firmware connects to the WiFi network using the
stored commissioning credentials and runs an on-device MQTT broker (PicoMQTT).
Turnout state is bridged bidirectionally:

- **State publication** (`loconet/turnout/<address>/state`): every time a
  turnout command is received from the LocoNet bus, the firmware publishes
  the resulting state (`"CLOSED"` or `"THROWN"`) to the corresponding MQTT
  topic. State is also re-published every 30 seconds for all known turnouts,
  ensuring late subscribers receive the current state even if they missed the
  original command (PicoMQTT's broker mode does not honor the MQTT retained
  flag, so periodic re-publication is the reliability mechanism).
- **Command subscription** (`loconet/turnout/<address>/set`): the firmware
  listens to these topics and translates MQTT messages containing `"CLOSED"`
  or `"THROWN"` into LocoNet `OPC_SW_REQ` turnout commands sent to the bus.

Turnout addresses are passed through unchanged — a command to address 123
on LocoNet maps to `loconet/turnout/123/state` and
`loconet/turnout/123/set` on MQTT.

## Architecture overview

Hexagonal (ports & adapters) architecture. Domain and application code has
zero dependency on Arduino or the vendor LocoNet library, even transitively
— it's compiled and tested under a host-native PlatformIO environment
(`pio test -e native`) with no ESP32 SDK on the include path at all.
Hardware-facing code is isolated behind interfaces and only implemented in
adapters, each guarded with `#ifdef ARDUINO`.

```
lib/Loco2MqttCore/src/
├── domain/         Level, LocoNetMessage, LocoNetAdapterConfig, ParsedCommand,
│                   CommandLineParser, BootMode, SetupFormRenderer,
│                   LineAssembler, TurnoutAddress, TurnoutPosition,
│                   TurnoutStateChanged, SetTurnoutPosition, DomainEvent,
│                   DomainCommand, MqttMessage, IncomingMqttMessage,
│                   PendingLocoNetSend, LocoNetChecksum
│                   — pure value objects and logic, no I/O
├── ports/          DigitalPin, LocoNetPort, MessageLog, ConfigStore, UartPort,
│                   DigitalInput, Clock, SetupModeRequestStore, RebootTrigger,
│                   MqttPort, LocoNetSendScheduler, LocoNetMessageDecoder,
│                   MqttEventEncoder, MqttCommandDecoder, LocoNetEncoder,
│                   ActivityIndicator, LineStream
│                   — interfaces only
├── application/    LocoNetMessageLogger, CommissioningSession,
│                   ButtonSetupModeTrigger, PendingLocoNetSendScheduler,
│                   LocoNetMessageRouter, MqttCommandRouter, ActivityLed,
│                   FlashingMessageLog
│                   — the real application services
├── turnout/        TurnoutLocoNetDecoder, TurnoutMqttEncoder,
│                   TurnoutMqttCommandDecoder, TurnoutLocoNetEncoder
│                   — domain-specific device bridging logic
└── adapters/       EspDigitalPin, LocoNetEsp32Port, SerialMessageLog,
                     NvsConfigStore, EspUartPort, SerialCommissioningAdapter,
                     EspDigitalInput, ArduinoClock, NvsSetupModeRequestStore,
                     EspRebootTrigger, WebFormCommissioningAdapter,
                     CaptivePortalServer, EspWifiPort, PicoMqttPort,
                     EspMdnsPort, LocoNetOverTcpCodec, LocoNetOverTcpPort,
                     WiFiClientLineStream
                     (#ifdef ARDUINO — the only files that touch real hardware
                     or the vendor library, except LocoNetOverTcpCodec/Port
                     which are natively tested — see "Current status" above)
test/support/       Hand-written fakes (no mocking framework) for native tests:
                     FakeDigitalPin, FakeLocoNetPort, FakeMessageLog,
                     FakeConfigStore, FakeUartPort, FakeDigitalInput,
                     FakeClock, FakeSetupModeRequestStore, FakeRebootTrigger,
                     FakeMqttPort, FakeLocoNetSendScheduler,
                     FakeActivityIndicator, FakeLineStream
src/main.cpp        Composition root — wires real adapters together; no
                     business logic
```

`LocoNetMessageLogger` was the original vertical slice: on each call to
`update()`, it drains every message currently waiting on `LocoNetPort` and
forwards each one to `MessageLog`. It's still used exactly that way during
bench-serial commissioning (`BootMode::NeedsCommissioning`), wired to
`LocoNetEsp32Port` → `SerialMessageLog`. Once commissioning is complete
(`BootMode::Normal`), `LocoNetMessageRouter` takes over as the sole reader
of `LocoNetPort` and does the same serial logging itself (via the
`MessageLog` it's constructed with) in addition to the MQTT bridging
described above — two separate objects both draining the same
LocoNet receive queue would starve one of them, so only one is ever active
per boot mode.

`ActivityLed` drives a `DigitalPin` high for a fixed window (`update()`,
ticked from `loop()`, drops it low again once the window elapses) and is the
one real implementation of the `ActivityIndicator` port. `FlashingMessageLog`
decorates whichever `MessageLog` a boot mode already constructs, so every
`record()` call — already made exactly once per received message, by
whichever object owns the sole `LocoNetPort::receive()` drain for that boot
mode — also calls `ActivityIndicator::flash()`. This is deliberate: adding a
second `receive()` consumer just to watch for traffic would race the
existing one for the same destructive-read queue (see the `LocoNetPort`
note in "Architecture overview" above), so the LED rides along on the
logging call that already happens instead.

`CommissioningSession` (driven by `SerialCommissioningAdapter` over the bench
serial console, or by `WebFormCommissioningAdapter`/`CaptivePortalServer` over
the wireless setup AP) is the WiFi-commissioning vertical slice added since —
see "WiFi commissioning" above for the two ways to drive it, and
`selectBootMode` (`domain/BootMode.h`) for how `src/main.cpp` decides which
path a boot takes.

For a deeper technical write-up — the vendor library's internal boot
sequence, why the receive queue is capped and how, exactly which malformed
frames get filtered and why, plus the GPIO0 strapping-pin hazard the
commissioning button trigger has to avoid — see [`CLAUDE.md`](CLAUDE.md),
which was written for an AI coding agent but doubles as the most detailed
technical reference in this repo.

The full implementation plans, in order, with the reasoning behind each
design decision, are at
[`docs/superpowers/plans/2026-09-04-loconet-esp32-adapter-scaffold.md`](docs/superpowers/plans/2026-09-04-loconet-esp32-adapter-scaffold.md)
(the original RX/TX/logging scaffold),
[`docs/superpowers/plans/2026-09-05-node-config-commissioning.md`](docs/superpowers/plans/2026-09-05-node-config-commissioning.md)
(WiFi commissioning), and
[`docs/superpowers/plans/2026-09-06-mqtt-turnout-bridge.md`](docs/superpowers/plans/2026-09-06-mqtt-turnout-bridge.md)
(the MQTT turnout bridge).

Architecture Decision Records live in `docs/decisions/`, numbered
sequentially — see
[`docs/decisions/0001-interim-jmri-loconet-over-tcp-transport.md`](docs/decisions/0001-interim-jmri-loconet-over-tcp-transport.md)
for the interim JMRI transport described in "Current status" above,
including its revert plan.

### Third-party dependencies

Pinned to specific commits in `platformio.ini` (both repos lack tagged
releases, so a commit pin is the closest thing to a stable version):

- [`LocoNetESP32HB`](https://github.com/tanner87661/LocoNetESP32HB) (also
  known as `IoTT_LocoNetHBESP32`) — the vendor LocoNet RX/TX library this
  firmware wraps.
- [`ArduinoJson`](https://github.com/bblanchon/ArduinoJson) — a transitive
  dependency of `LocoNetESP32HB` that its own package manifest doesn't
  declare, so it's pinned here explicitly or the build fails on a missing
  header.

## Known limitations

- **Turnout is the only device type bridged.** LocoNet sensors, transponders,
  and other device types remain future work. Only turnout state commands
  (`OPC_SW_REQ`/`OPC_SW_REP`) are decoded, published, and subscribed to on
  MQTT.
- **The MQTT broker accepts unauthenticated connections.** Anyone who can
  reach it over WiFi can publish `loconet/turnout/<address>/set` and drive
  real turnout hardware — deliberate for a home-layout use case (see the
  design spec), but worth knowing before putting this on a shared network.
- **The current transport depends on a running, reachable JMRI instance.**
  This is temporary (see "Current status" above and the ADR's revert
  plan) — the electrical LocoNet interface doesn't need JMRI at all, but
  isn't wired into `src/main.cpp` right now.
- **The vendor library can itself write past its own 48-byte message
  buffer** on a malformed long-form frame before this adapter's code ever
  runs — this firmware clamps every copy it makes into and out of that
  buffer, but can't fix the vendor's own internal buffer fill.
- **The receive queue holds at most 32 undelivered messages**, dropping the
  oldest first if the main loop falls behind. On a real LocoNet bus at
  normal traffic levels this should never be visible; it would only matter
  if something slow (like a network call) shared the same loop and stalled
  draining for a while.
- **Not tested against real hardware yet** (see the hardware requirements
  section above) — build-checks and native tests only, so far.

## Troubleshooting

**Native tests won't run (but do build) on Windows, failing with a status
like `0xC0000139` (`STATUS_ENTRYPOINT_NOT_FOUND`):** a stale
`libstdc++-6.dll` earlier on `PATH` is shadowing the MinGW runtime the test
binary was linked against. Put your MinGW `bin` directory ahead of any other
GCC/MinGW installation on `PATH`.

**Nothing shows up on the serial monitor after flashing:** confirm the RJ12
is actually seated in an open LocoNet port, and work through the RX-stage
troubleshooting in `docs/breadboard-build-guide.md` (it covers checking R1
polarity, the opto's Enable pin, and R2's pull-up target) before suspecting
the firmware.

**You see hex output, but it looks garbled or misaligned:** double-check
`InverseLogic` — this is the first thing to flip if you ever swap to a
different opto circuit. Also confirm the RX pin is a genuine hardware UART
pin, not a software-serial pin.

**Sending a message seems to jam the bus or make other devices error out:**
almost certainly the TX-pin boot-safety issue described above — verify the
TX GPIO doesn't glitch high during an ESP32 reset with a scope or meter
before connecting to a live bus again.

## Development

```bash
pio test -e native                  # fast native unit tests, no hardware
pio test -e native -f test_<name>   # a single test suite
pio run -e esp32dev                 # build-check firmware (no upload)
pio run -e esp32dev --target upload # flash real hardware
pio device monitor                  # serial monitor, 115200 baud
```

This project was built with strict TDD: every behavior-bearing class has a
native test written and confirmed failing before the code that makes it
pass. Test doubles are hand-written fakes in `test/support/` — there's no
mocking framework anywhere in this codebase. See `CLAUDE.md` for the full
list of engineering constraints this project holds itself to (immutability,
method-size limits, dependency-direction rules, and why).
