# Loco2MQTT

Firmware for an ESP32-based LocoNet adapter.

**Current status: Phase 1.** This firmware proves a working RX/TX interface
to a LocoNet bus, built on a breadboard opto/transistor interface (see
[`docs/breadboard-build-guide.md`](docs/breadboard-build-guide.md)), and logs
every received LocoNet message to the serial console. **MQTT bridging — the
project's eventual purpose — has not been started yet.** There is no network
stack, no broker connection, and no MQTT topic scheme in this firmware today.
If you're looking for a drop-in LocoNet-to-MQTT gateway, this isn't one yet;
if you want to watch this project's LocoNet plumbing work and grow toward
that, read on.

Built with Test-Driven Development and Hexagonal Architecture — domain and
application logic is tested natively on a desktop, independent of the ESP32
hardware and independent of any physical LocoNet bus.

## What it does today

- Receives LocoNet messages over a breadboard-built RX interface and prints
  each one to the serial console as space-separated uppercase hex
  (`B2 00 00 50`).
- Validates every received frame against the vendor library's own
  collision/checksum/incomplete-frame flags before logging it, so bus noise
  or wiring mistakes don't show up disguised as real traffic.
- Can transmit a `LocoNetMessage` onto the bus (`LocoNetPort::send()` is
  implemented and hardware-build-checked), but nothing in the firmware calls
  it yet — there's no outbound message source until MQTT (or something else)
  is wired up to drive it.

## What it doesn't do yet

- No MQTT client, no broker connection, no topic scheme. (WiFi credentials
  *are* now configurable — see "WiFi commissioning" below — but nothing
  yet uses the network they connect to.)
- No JMRI integration beyond what a raw LocoNet tap gives you for free.
- No use of `LocoNetPort::send()` from any application logic.
- No PCB — this runs on a breadboard interface only (a PCB phase is noted
  as future work in the build guide).

## Hardware requirements

- An ESP32 dev board. This project has been built and tested against an
  ELEGOO ESP-WROOM-32 module; other ESP32 boards should work but haven't
  been verified.
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
6 suites should pass. This is the fast feedback loop for any code change;
run it before touching real hardware.

### 2. Build the firmware

```bash
pio run -e esp32dev
```

This compiles the real firmware against the ESP32/Arduino toolchain and the
vendor LocoNet library. It does **not** require a board to be plugged in —
it's a pure build-and-link check.

### 3. Wire the hardware

Follow [`docs/breadboard-build-guide.md`](docs/breadboard-build-guide.md)
from the top. It's written to be built and bench-verified in stages (RX
alone, then TX alone, then the full loop) rather than all at once — don't
skip the intermediate multimeter checks even if you're confident, since a
wiring mistake here can affect other devices on a shared LocoNet bus.

### 4. Flash it

```bash
pio run -e esp32dev --target upload
```

### 5. Watch it work

```bash
pio device monitor
```

(115200 baud.) Connect the adapter's RJ12 to an open LocoNet port. You
should immediately see hex-formatted lines for bus traffic — heartbeats,
throttle activity, anything else already on the bus. If you send a message
from another LocoNet device (or via JMRI), it should appear as a new line.
Seeing plausible-looking hex here confirms wiring and `InverseLogic` are
correct — see Troubleshooting if you see nothing, or garbage.

To run a single native test file instead of the whole suite:

```bash
pio test -e native -f test_<name>
```

## Configuration reference

Everything hardware-specific lives in one of two places:

| Setting | Where | Value | Why |
|---|---|---|---|
| RX pin | `src/main.cpp`, `kLocoNetRxPin` | `16` (UART2 RX) | Must be a hardware UART-capable pin — LocoNet's 16.66kbps timing needs the real UART, not a bit-banged read. |
| TX pin | `src/main.cpp`, `kLocoNetTxPin` | `17` | Any free GPIO (TX is bit-level timed in software), but **must be boot-safe** — see the warning below. |
| `InverseLogic` | `lib/Loco2MqttCore/src/adapters/LocoNetEsp32Port.cpp`, `LocoNetEsp32Port`'s constructor | hardcoded `true` | This breadboard circuit's 6N137 opto output is inverted; not a runtime option, since it's a property of the physical circuit, not a config choice. |
| Serial baud rate | `src/main.cpp`, `kSerialBaudRate` | `115200` | Console/logging baud rate — unrelated to LocoNet's own 16.66kbps bus speed. |
| Receive queue cap | `lib/Loco2MqttCore/src/adapters/LocoNetEsp32Port.cpp`, `kMaxPendingMessages` | `32` | Bounds worst-case heap growth if messages arrive faster than the main loop drains them; oldest messages are dropped first once full. |

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

## Architecture overview

Hexagonal (ports & adapters) architecture. Domain and application code has
zero dependency on Arduino or the vendor LocoNet library, even transitively
— it's compiled and tested under a host-native PlatformIO environment
(`pio test -e native`) with no ESP32 SDK on the include path at all.
Hardware-facing code is isolated behind interfaces and only implemented in
adapters, each guarded with `#ifdef ARDUINO`.

```
lib/Loco2MqttCore/src/
├── domain/         Level, LocoNetMessage — pure value objects, no I/O
├── ports/          DigitalPin, LocoNetPort, MessageLog — interfaces only
├── application/     LocoNetMessageLogger — the one real application service
└── adapters/       EspDigitalPin, LocoNetEsp32Port, SerialMessageLog
                     (#ifdef ARDUINO — the only files that touch real hardware
                     or the vendor library)
test/support/       Hand-written fakes (no mocking framework) for native tests
src/main.cpp        Composition root — wires real adapters together; no
                     business logic
```

`LocoNetMessageLogger` is the whole current vertical slice: on each call to
`update()`, it drains every message currently waiting on `LocoNetPort` and
forwards each one to `MessageLog`. Wired to real hardware in `src/main.cpp`
as `LocoNetEsp32Port` (RX/TX over the breadboard interface) →
`SerialMessageLog` (prints to the Arduino `Serial` console).

For a deeper technical write-up — the vendor library's internal boot
sequence, why the receive queue is capped and how, exactly which malformed
frames get filtered and why — see [`CLAUDE.md`](CLAUDE.md), which was
written for an AI coding agent but doubles as the most detailed technical
reference in this repo.

The full implementation plan (all 9 build tasks, in order, with the
reasoning behind each design decision) is at
[`docs/superpowers/plans/2026-09-04-loconet-esp32-adapter-scaffold.md`](docs/superpowers/plans/2026-09-04-loconet-esp32-adapter-scaffold.md).

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

- **`LocoNetPort::send()` has no caller.** It's implemented and
  build-checked, but no application logic drives it yet — sending will
  matter once MQTT (or anything else) needs to originate LocoNet traffic.
- **The vendor library can itself write past its own 48-byte message
  buffer** on a malformed long-form frame before this adapter's code ever
  runs — this firmware clamps every copy it makes into and out of that
  buffer, but can't fix the vendor's own internal buffer fill.
- **The receive queue holds at most 32 undelivered messages**, dropping the
  oldest first if the main loop falls behind. On a real LocoNet bus at
  normal traffic levels this should never be visible; it would only matter
  if something slow (like a future blocking network call) shared the same
  loop and stalled draining for a while.
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
