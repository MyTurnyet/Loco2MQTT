# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Purpose

Loco2MQTT — firmware for an ESP32-based LocoNet adapter. Phase 1 (this
scaffold) proves RX/TX over a breadboard-built opto/transistor interface
(`docs/breadboard-build-guide.md`) with one working vertical slice: logging
every received LocoNet message to serial. MQTT bridging is future work, not
yet started.

## Commands

```bash
pio test -e native                  # run host-native unit tests (no hardware needed)
pio run -e esp32dev                 # build the firmware for the ESP32
pio run -e esp32dev --target upload
pio device monitor                  # serial monitor, 115200 baud
```

To run a single native test file:
```bash
pio test -e native -f test_<name>
```

There is no `native` build target for `src/` (`test_build_src = false` in
`platformio.ini`) — native test binaries only compile `test/` plus whatever
`lib/` code they include, not `main.cpp`.

**If `pio test -e native` fails to *run* (not compile) with a Windows status
like `0xC0000139` / `STATUS_ENTRYPOINT_NOT_FOUND`:** this is a stale
`libstdc++-6.dll` earlier on `PATH` shadowing the MinGW runtime the test
binary was linked against, not a code problem. Make sure your MinGW `bin`
directory is ahead of any other GCC/MinGW installs on `PATH` before running
tests.

## Architecture

Hexagonal architecture, modeled on `D:\Development\MaltbeeTurnoutController`
and `D:\Development\MaltbeeController`. The critical rule: **domain and
application code must compile and run under the `native` PlatformIO
environment without `Arduino.h`.** Hardware-specific code is isolated behind
ports (interfaces) and only implemented in adapters.

- **Ports** (`lib/Loco2MqttCore/src/ports/`) are pure interfaces the
  domain/application depend on: `DigitalPin`, `LocoNetPort`, `MessageLog`.
- **Adapters** (`lib/Loco2MqttCore/src/adapters/`) implement ports against
  real ESP32/LocoNetESP32HB hardware, guarded with `#ifdef ARDUINO` so they
  don't break the native build. Hand-written test doubles (`test/support/`)
  implement the same ports for native unit tests — no mocking framework.
- **`src/main.cpp` is the composition root only** — it wires adapters and
  application objects together and calls non-blocking `update()` methods
  from `loop()`. No business logic lives here. File-scope object
  construction in `main.cpp` is the one accepted exception to "no globals" —
  Arduino's `setup()`/`loop()` model has no other place to hold constructed
  objects across calls.
- **Actual boot sequence for `LocoNetEsp32Port`:** the vendor's
  `LocoNetESPSerial` constructor self-initializes hardware (UART begin,
  timer setup) whenever both pins passed to it are non-negative — confirmed
  in `IoTT_LocoNetHBESP32.cpp`. Because of that, `LocoNetEsp32Port::begin()`
  is a documented no-op (calling `serial_.begin()` again would re-run
  hardware init a second time — duplicate timer setup, duplicate UART
  begin, a leaked interrupt handle). `setup()` in `src/main.cpp` does not
  call it. The method is kept on the class only so the adapter's
  `begin()`/`update()` shape stays stable; it does nothing today.
- `LocoNetEsp32Port` (`lib/Loco2MqttCore/src/adapters/LocoNetEsp32Port.cpp`)
  bridges the vendor library's plain-C-callback receive API into a
  translation-unit-local queue — a deliberate, narrowly-scoped exception to
  "no statics," matching the existing pattern in
  `MaltbeeController`'s `MrrwaLocoNetFeedbackSource.cpp`. Only one instance
  may exist at a time. The queue is capped (32 entries, drop-oldest) since
  it is heap-allocated and otherwise unbounded.
  The vendor callback (`onLocoNetMessageReceived`, invoked via
  `processLNMsg`/`handleLNIn`) runs synchronously inside
  `LocoNetESPSerial::processLoop()`, which this adapter's `update()` calls
  from `loop()` — **not from an ISR** — which is why it's safe for the
  callback to push onto a heap-allocating `std::queue`.
  The vendor's own max message size is `lnMaxMsgSize` (`IoTTCommDef.h`) =
  48 bytes, the fixed size of both `lnReceiveBuffer::lnData` and
  `lnTransmitMsg::lnData`. The vendor library can itself report an
  `lnMsgSize` beyond that 48-byte buffer on a malformed long-form frame
  (`handleLNIn`'s long-message branch trusts an attacker/noise-controlled
  length byte, up to 127, without validating it against `lnMaxMsgSize`) —
  this adapter clamps every copy into/out of those buffers with
  `std::min<size_t>(..., lnMaxMsgSize)` to avoid reading or writing past
  them. The callback also checks `lnReceiveBuffer::errorFlags` and discards
  (does not enqueue) any frame with `errorCollision`, `errorFrame`,
  `errorTimeout`, `errorCarrierLoss`, `msgIncomplete`, `msgXORCheck`, or
  `msgStrayData` set, so malformed frames are never logged as if valid.
- Classes are built needs-driven: value objects and pure domain logic first,
  then ports and fakes, then the one real application service, then
  adapters, then the composition root last.

### Current source layout

- `lib/Loco2MqttCore/src/domain/` — `Level`, `LocoNetMessage`
- `lib/Loco2MqttCore/src/ports/` — `DigitalPin`, `LocoNetPort`, `MessageLog`
- `lib/Loco2MqttCore/src/application/` — `LocoNetMessageLogger`
- `lib/Loco2MqttCore/src/adapters/` — `EspDigitalPin`, `LocoNetEsp32Port`,
  `SerialMessageLog`
- `test/support/` — `FakeDigitalPin`, `FakeLocoNetPort`, `FakeMessageLog`
- `test/test_<name>/test_main.cpp` — Catch2 test binaries

**Why `native`'s `build_flags` includes `-Ilib/Loco2MqttCore/src`:**
PlatformIO's dependency finder only adds a `lib/` folder to the include path
when some already-compiled file references it directly. A test file that
only includes `support/FakeX.h` (which in turn includes `ports/X.h`) never
triggers that discovery, since the fake isn't itself a recognized library
dependency — so `ports/X.h` wouldn't resolve without the explicit `-I`. Keep
this flag when adding new ports.

## Engineering Principles

- **TDD**: write a failing native test first, implement the minimum to
  pass, refactor only while green.
- **Dependency inversion**: domain/application depend on ports; adapters
  depend on domain-owned interfaces — never the reverse. No statics or
  globals outside `main.cpp` and the one documented vendor-callback bridge
  above; everything else is constructed and injected via the composition
  root.
- **No mocking frameworks**: hand-written fakes only, in `test/support/`.
- **Ask, don't tell**: ports expose behavior, not raw state for callers to
  branch on.
- **Immutable by default**: `LocoNetMessage` and `Level` are immutable;
  mutability is confined to adapters that genuinely own hardware state.
- **Single responsibility, small interfaces**: 1–2 methods per port, one job
  per class. Methods ≤ 8 lines, cognitive complexity < 4 — split anything
  bigger.
