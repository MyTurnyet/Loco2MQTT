# Firmware Version Number — Design

## Overview

Loco2MQTT has no firmware version number today — there's no way to tell,
just by looking at the device, what code it's running. This adds a single
version constant that's visible in two places: the wireless-setup AP
configuration page, and the serial console at boot.

## Format

A manually-maintained semantic version string, starting at `0.1.0`. There's
no build tooling deriving it from git — it's a single constant you bump by
hand when you make a notable change. This fits a hobby project with one
maintainer and no release automation; a git-derived version would need a
build script and wouldn't produce a consistent value between the `native`
and `esp32dev` environments.

## Component

```cpp
// lib/Loco2MqttCore/src/domain/FirmwareVersion.h
#pragma once

constexpr const char* kFirmwareVersion = "0.1.0";
```

A single header-only constant in `domain/` — no `.cpp`, no dependencies. It
lives in `domain/` because it's a pure value other code depends on, not the
reverse: `SetupFormRenderer` (an existing domain-layer pure function) and
`src/main.cpp` (the composition root) both consume it, and neither should
have to reach into the other to get it.

## Where it's shown

**1. The AP configuration page.** `renderSetupForm()`
(`lib/Loco2MqttCore/src/domain/SetupFormRenderer.cpp`) already renders the
wireless-setup form as a pure, natively-testable function. It gains one new
line of HTML near the top of the page body — `Loco2MQTT v0.1.0` — built
from `kFirmwareVersion`, so anyone connecting to the `Loco2MQTT-Setup` AP to
configure WiFi can see at a glance what firmware version is running.

**2. The serial console at boot.** `src/main.cpp`'s `setup()` prints the
version once, immediately after `Serial.begin(kSerialBaudRate)`:

```cpp
Serial.begin(kSerialBaudRate);
Serial.print("Loco2MQTT v");
Serial.println(kFirmwareVersion);
```

This runs before `selectBootMode()` and any other setup work, so the
version line is the first thing to appear in `pio device monitor` output on
every boot, regardless of `BootMode`.

## Testing

- **`test/test_setup_form_renderer/test_main.cpp`** gains one new test case
  asserting the rendered page contains `kFirmwareVersion`'s value, so a
  future refactor or version bump that breaks the render is caught by the
  native suite.
- The `main.cpp` boot-print is build-check-only (`pio run -e esp32dev`) —
  `src/main.cpp` has no native test target (`test_build_src = false` in
  `platformio.ini`), matching how every other composition-root change in
  this project has been verified.

## Explicitly out of scope

- No git-derived or build-time-generated version (see "Format" above).
- No version display anywhere else (README, MQTT topics, HTTP headers) —
  just the two places requested.
- No changelog or version-history tracking; bumping `kFirmwareVersion` is a
  manual, one-line edit whenever it's warranted.
