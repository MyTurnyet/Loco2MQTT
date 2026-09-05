# Loco2MQTT

Firmware for an ESP32-based LocoNet adapter. Phase 1: a working RX/TX
interface to a LocoNet bus, proven on a breadboard (see
`docs/breadboard-build-guide.md`) and driven by this firmware's first
vertical slice — logging every received LocoNet message to serial.

Built with Test-Driven Development and Hexagonal Architecture — domain and
application logic is tested natively on a desktop, independent of the ESP32
hardware.

## Requirements

- PlatformIO
- An ESP32 dev board wired per `docs/breadboard-build-guide.md`
- C++17 compiler (for native tests)

## Folder layout

- `lib/Loco2MqttCore/src/domain/` — pure value objects and domain logic
  (`Level`, `LocoNetMessage`); no Arduino dependency.
- `lib/Loco2MqttCore/src/ports/` — interfaces the domain/application depend
  on (`DigitalPin`, `LocoNetPort`, `MessageLog`).
- `lib/Loco2MqttCore/src/application/` — application services that consume
  ports (`LocoNetMessageLogger`); no Arduino dependency.
- `lib/Loco2MqttCore/src/adapters/` — real hardware implementations of the
  ports (`EspDigitalPin`, `LocoNetEsp32Port`, `SerialMessageLog`). The only
  files allowed to `#include <Arduino.h>` or the LocoNet library, guarded
  with `#ifdef ARDUINO` so the `native` build ignores them.
- `test/support/` — hand-written fakes (`FakeDigitalPin`, `FakeLocoNetPort`,
  `FakeMessageLog`) implementing the same ports for native unit tests. No
  mocking framework.
- `test/test_<name>/test_main.cpp` — Catch2 test binaries.
- `src/main.cpp` — the composition root. Wires real adapters together; no
  business logic.

## Building and testing

```bash
pio test -e native                  # run native unit tests, no hardware needed
pio run -e esp32dev                 # build the firmware
pio run -e esp32dev --target upload
pio device monitor                  # serial monitor, 115200 baud
```

To run a single native test file:
```bash
pio test -e native -f test_<name>
```

On Windows, if `pio test -e native` builds but the test binary fails to
launch with a status like `0xC0000139` (`STATUS_ENTRYPOINT_NOT_FOUND`), a
stale `libstdc++-6.dll` earlier on `PATH` is shadowing the MinGW runtime the
test binary was linked against — put your MinGW `bin` directory first on
`PATH`.

## Hardware configuration

See `docs/breadboard-build-guide.md` for the full circuit (6N137 opto RX
stage, 2N3904 open-collector TX driver, RJ12 wiring). The two
hardware-specific settings the guide calls out live in exactly one place,
`src/main.cpp`:

- **`InverseLogic`** — hardcoded `true` inside `LocoNetEsp32Port`'s
  constructor (`lib/Loco2MqttCore/src/adapters/LocoNetEsp32Port.cpp`). This
  circuit's 6N137 opto output is always inverted; it is not a runtime
  option.
- **RX pin (`kLocoNetRxPin`, `src/main.cpp`)** — must be a hardware
  UART-capable pin (default: GPIO16 / UART2 RX). LocoNet's 16.66kbps timing
  requires the hardware UART, not a bit-banged read.
- **TX pin (`kLocoNetTxPin`, `src/main.cpp`)** — any free GPIO (TX is
  bit-level timed in software by the library), but it **must be boot-safe**:
  a GPIO that is ever observed high across an ESP32 reset will momentarily
  turn on the 2N3904 and pull the whole shared LocoNet bus low, disrupting
  every other device on the layout. The default, GPIO17, is not one of the
  ESP32's documented strapping pins (GPIO0, 2, 5, 12, 15) — but confirm
  against your specific dev board's datasheet, and verify with a scope or
  meter through a reset cycle before connecting to a live bus, per the build
  guide's own troubleshooting section.

## Status

First vertical slice complete: `LocoNetMessageLogger` polls `LocoNetPort`
and forwards every received message to `MessageLog`, wired to real hardware
in `src/main.cpp` as `LocoNetEsp32Port` → `SerialMessageLog`. Sending
messages (`LocoNetPort::send()`) is implemented and build-checked but not
yet driven by any application logic. MQTT bridging (the project's eventual
purpose) is not yet started.
