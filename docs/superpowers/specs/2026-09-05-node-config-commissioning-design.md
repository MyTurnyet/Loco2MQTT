# NodeConfig & Commissioning — Design

Date: 2026-09-05

## 1. Scope

Loco2MQTT's eventual MQTT bridge needs WiFi to bring its network stack up.
This project must not hardcode WiFi credentials into the firmware — they
must be configurable per-deployment, the way the sibling projects
(`MaltbeeController`, `MaltbeeTurnoutController`) already do it. This spec
covers commissioning only: getting a WiFi SSID and password stored on the
device, persistently, through two front doors:

- **Bench-serial**: a text command console over the same USB-serial
  connection used for `pio device monitor`, for commissioning at a desk
  before the board is installed on the layout.
- **Wireless setup**: triggered by holding the ESP32's BOOT button (GPIO0)
  for 3 seconds, which reboots the device into an open WiFi access point
  serving a captive-portal web form, for commissioning (or re-commissioning)
  once the board is already mounted and a USB cable isn't convenient.

Both paths write to the same persisted config, read by the same domain
object, so nothing downstream needs to know which path was used.

This project deliberately does **not** touch MQTT, the on-device broker, or
LocoNet↔MQTT translation — see Section 9.

## 2. Config domain object

`LocoNetAdapterConfig` is an immutable value object holding exactly two
fields: `wifiSsid` and `wifiPassword` (both `std::string`). No broker
address, port, or credentials — the MQTT bridge (a separate future
sub-project) runs its own broker on-device rather than connecting out to an
external one, so there is nothing broker-related left to configure. A
`isComplete()` query answers whether both fields are non-empty, used by
`BootModeSelector` (Section 6) to decide whether the device can boot
normally.

`ConfigStore` is the port: `load()` returns the persisted
`LocoNetAdapterConfig` (default-constructed/empty if nothing has been saved
yet), `save(const LocoNetAdapterConfig&)` persists it. `NvsConfigStore` is
the ESP32 adapter, backed by the `Preferences` library (part of the Arduino
core — no new dependency). `FakeConfigStore` is an in-memory hand-written
fake for native tests.

Per your note that more config fields will likely be added later (topic
prefix, retained-message behavior, etc. once the MQTT sub-project starts):
`LocoNetAdapterConfig` is intentionally just a plain field-holding value
object with `with...()` copy-returning mutators (matching this project's
existing `Level`/`LocoNetMessage` convention), and `ConfigStore` persists
the whole object opaquely (as key-value pairs under fixed NVS keys). Adding
a field later means adding one field, one NVS key, and one line each in the
serial parser and the web form — no structural change to the port or the
store.

## 3. Bench-serial commissioning

A tiny line-oriented command console read over the same serial port as the
firmware's existing logging output.

- `UartPort` (new port): `readLine()` returns `std::optional<std::string>`
  (one line, no trailing newline, or `nullopt` if no complete line is
  buffered yet) and `writeLine(const std::string&)` for prompts/echoes.
  `EspUartPort` wraps Arduino `Serial`; `FakeUartPort` is a hand-written
  fake fed lines directly in tests.
- `ParsedCommand` (domain value object) and `CommandLineParser` (domain,
  pure function) turn a raw line into a typed command: `SetSsid(value)`,
  `SetPassword(value)`, `Show`, `Save`, or `Unknown(rawLine)`. Malformed
  input (empty line, unrecognized verb) becomes `Unknown`, handled
  uniformly by the session rather than needing per-command validation.
- `CommissioningSession` (application) owns the in-progress
  (not-yet-saved) `LocoNetAdapterConfig`, applies each `ParsedCommand` to
  it, and calls `ConfigStore::save()` only on an explicit `Save` command —
  never automatically, so a typo doesn't silently persist. `Show` echoes
  the current in-progress SSID but never the password (see Section 5's
  security rule — it applies here too). For the same reason, an `Unknown`
  command's response never echoes the raw input line back — a misspelled
  verb (`set-passwrod hunter2`) or a rejected overlong line could contain
  a password typed as an argument, so the reply is always a fixed generic
  message.
- `SerialCommissioningAdapter` drives the loop: reads a line via
  `UartPort`, parses it, hands the result to `CommissioningSession`,
  writes a response line. Lines longer than `kMaxLineLength = 128` bytes
  are rejected as `Unknown` rather than buffered without bound.

## 4. Wireless setup trigger

- `DigitalInput` (new port, read-only — distinct from the existing
  write-only `DigitalPin`): `isActive() const` reports the current level
  of one GPIO. `EspDigitalInput` reads a real pin (GPIO0, internal
  pull-up, active-low per the BOOT button's wiring); `FakeDigitalInput`
  is a settable fake for tests.
- `Clock` (new port): `nowMilliseconds() const` returns elapsed
  milliseconds since boot. `ArduinoClock` wraps `millis()`;
  `FakeClock` is a settable fake.
- `ButtonSetupModeTrigger` (application), ported from MaltbeeController's
  equivalent — including its release-based triggering, not simplified
  away: on each `update()` call it tracks a hold starting when
  `DigitalInput::isActive()` becomes true, then a release starting when it
  goes false, and only fires once the release has held steady for
  `kReleaseSettleMs = 50` (debouncing a bounced release) *and* the
  preceding hold lasted at least `kHoldDurationMs = 3000`. On firing, it
  calls `SetupModeRequestStore::request()` once and returns `true` so the
  composition root knows to reboot. **Triggering on the hold itself (as
  an earlier revision of this spec specified) is wrong on this hardware**:
  GPIO0 is a strapping pin, and `ESP.restart()` while it reads low drops
  the ROM bootloader into permanent UART download mode instead of
  re-running application code. Triggering only after a confirmed release
  guarantees the pin has already returned high before any reset happens.
- `SetupModeRequestStore` (port): `request()` persists a one-shot flag,
  `consumeIfRequested()` reads and clears it in a single call (so a normal
  reboot afterward doesn't re-enter setup mode forever).
  `NvsSetupModeRequestStore` is the ESP32 adapter (a single NVS boolean
  key); `FakeSetupModeRequestStore` (`test/support/`) is the hand-written
  fake used by `ButtonSetupModeTrigger`'s and `BootModeSelector`'s own
  tests.

## 5. Captive portal

- `CaptivePortalServer` (adapter): starts an open (no passphrase) WiFi
  access point with a fixed name, `"Loco2MQTT-Setup"` — fixed rather than
  MAC-derived, since this is a single-device project with no risk of two
  APs colliding on one workbench. Runs a `DNSServer` that answers every
  DNS query with the AP's own IP (the standard captive-portal trick so
  phones/laptops auto-open the setup page) and a `WebServer` serving one
  page. Registers an `onNotFound` handler (routed to the same page as
  `GET /`) in addition to the explicit `/` route — OS captive-portal
  detection probes arbitrary paths (`/generate_204`,
  `/hotspot-detect.html`, ...) and a bare 404 on those is exactly the
  signal that tells the OS there's no portal to show, so auto-open would
  silently never fire without it.
- `SetupFormRenderer` (domain, pure function): given the current
  `LocoNetAdapterConfig`, renders the HTML form. **Security rule: the
  stored WiFi password is never reflected back into the form** — the
  password field always renders empty, so submitting the form without
  retyping the password would clear it. (This matches the pattern in
  MaltbeeController's equivalent form.)
- `WebFormCommissioningAdapter` (adapter): handles the form POST, builds a
  `LocoNetAdapterConfig` from the submitted fields, saves it via
  `ConfigStore`, and triggers a reboot back into normal boot mode. It does
  not persist a new "please enter setup mode" request — the reboot should
  land in `Normal` or `NeedsCommissioning` per `BootModeSelector`, not
  loop back into `WirelessSetup`. A separate `wouldAccept()` query lets
  the caller check completeness before committing to a response: since
  `ESP.restart()` never returns, `CaptivePortalServer` must send the HTTP
  response to the browser *before* calling the save-and-reboot path, not
  after — sending it after a successful submission would be unreachable
  dead code (the browser just sees a dropped connection), and sending a
  generic "Saved" response unconditionally would falsely claim success
  for a rejected submission.

## 6. Composition root wiring

- `BootMode` (domain enum): `Normal`, `NeedsCommissioning`,
  `WirelessSetup`.
- `BootModeSelector` (domain, pure function): given
  `LocoNetAdapterConfig::isComplete()` and whether
  `SetupModeRequestStore::consumeIfRequested()` returned true, returns the
  `BootMode` — `WirelessSetup` if the setup request was pending
  (regardless of config completeness, so re-commissioning an
  already-configured device works), else `NeedsCommissioning` if config is
  incomplete, else `Normal`.
- `src/main.cpp`'s `setup()` reads `BootMode` once at startup and branches:
  - `WirelessSetup`: constructs and starts `CaptivePortalServer` +
    `WebFormCommissioningAdapter`; `loop()` calls only the portal's
    `update()` and returns early — the existing LocoNet-logging vertical
    slice (`locoNetPort`/`messageLog`/`logger`) is not constructed at all
    in this mode, since the radio is busy running an AP rather than
    joining the layout's WiFi.
  - `Normal` and `NeedsCommissioning`: the existing LocoNet-logging
    vertical slice is constructed and runs in `loop()` exactly as it does
    today, unchanged. `NeedsCommissioning` additionally constructs
    `SerialCommissioningAdapter` so the bench-serial console is available;
    `Normal` does not (nothing to commission), matching the principle
    that a device that already has good config doesn't need to expose a
    write path to it on every boot.
  - `ButtonSetupModeTrigger` runs in `loop()` in both `Normal` and
    `NeedsCommissioning` modes (never in `WirelessSetup` — the AP is
    already up), so a fully-commissioned device on the layout can still
    be walked over to and re-commissioned by holding BOOT.

No new PlatformIO dependencies: `WebServer`, `DNSServer`, `WiFi`, and
`Preferences` are all part of the ESP32 Arduino core already pinned by the
`esp32dev` environment.

## 7. Testing & error handling

Every new class above is native-testable except the five genuine hardware
shims (`NvsConfigStore`, `EspUartPort`, `EspDigitalInput`, `ArduinoClock`,
`CaptivePortalServer`), which get build-check-only verification via
`pio run -e esp32dev`, matching this project's existing convention for
`LocoNetEsp32Port`/`SerialMessageLog`/`EspDigitalPin`.

Native test plan, one Catch2 suite per class:

- `LocoNetAdapterConfig`: construction, `with...()` mutators,
  `isComplete()` true/false cases.
- `CommandLineParser`: each command verb, malformed/empty/unknown-verb
  input all producing `Unknown`.
- `CommissioningSession`: applying each `ParsedCommand` type, `Save` only
  persisting on explicit command, `Show` never exposing the password.
- `SerialCommissioningAdapter`: line assembly via `FakeUartPort`
  (including the `kMaxLineLength` rejection case), one full
  set-ssid/set-password/save round trip.
- `ButtonSetupModeTrigger`: hold under threshold (no trigger), hold past
  threshold (calls `SetupModeRequestStore::request()` once, not
  repeatedly, on the same continuous hold), release-and-reset.
- `BootModeSelector`: all three `BootMode` outcomes, including the
  "setup requested but config already complete" case.
- `SetupFormRenderer`: renders current SSID, always renders an empty
  password field regardless of stored value.
- `WebFormCommissioningAdapter`: valid submission saves and reboots;
  malformed/missing-field submission is rejected without saving a
  partial config.

Error handling stance, consistent with this project's existing
`LocoNetEsp32Port` conventions: validate at the boundary (the parser
rejects malformed serial input; the web adapter rejects malformed form
submissions) so nothing downstream needs defensive re-checking. No
retries, no timeouts beyond what's already specified (the 3-second BOOT
hold) — failure modes here are user-facing (bad input gets a rejection
message) rather than needing recovery logic.

## 8. File layout

```
lib/Loco2MqttCore/src/
├── domain/
│   ├── LocoNetAdapterConfig.h/.cpp        (wifiSsid + wifiPassword only)
│   ├── ParsedCommand.h
│   ├── CommandLineParser.h/.cpp
│   ├── BootMode.h                         (BootMode enum + BootModeSelector)
│   └── SetupFormRenderer.h/.cpp
├── ports/
│   ├── ConfigStore.h
│   ├── UartPort.h
│   ├── DigitalInput.h                     (new — read-only GPIO, distinct from existing write-only DigitalPin)
│   ├── Clock.h                            (new)
│   └── SetupModeRequestStore.h
├── application/
│   ├── CommissioningSession.h/.cpp
│   └── ButtonSetupModeTrigger.h/.cpp
└── adapters/
    ├── NvsConfigStore.h/.cpp              (#ifdef ARDUINO)
    ├── EspUartPort.h/.cpp                 (#ifdef ARDUINO)
    ├── EspDigitalInput.h                  (#ifdef ARDUINO)
    ├── ArduinoClock.h                     (#ifdef ARDUINO)
    ├── NvsSetupModeRequestStore.h/.cpp    (#ifdef ARDUINO)
    ├── SerialCommissioningAdapter.h/.cpp
    ├── WebFormCommissioningAdapter.h/.cpp
    └── CaptivePortalServer.h/.cpp         (#ifdef ARDUINO)

test/support/
├── FakeConfigStore.h
├── FakeUartPort.h
├── FakeDigitalInput.h
├── FakeClock.h
└── FakeSetupModeRequestStore.h

test/test_<name>/test_main.cpp   — one Catch2 suite per native-testable class
                                    listed in Section 7
```

Build order (strict TDD, each class green before starting the next):
`LocoNetAdapterConfig` → `CommandLineParser`/`ParsedCommand` →
`CommissioningSession` (with `FakeConfigStore`) →
`SerialCommissioningAdapter` (with `FakeUartPort`) → `DigitalInput`/`Clock`
ports + fakes → `ButtonSetupModeTrigger` → `SetupModeRequestStore` +
`FakeSetupModeRequestStore` → `BootMode`/`BootModeSelector` →
`SetupFormRenderer` → `WebFormCommissioningAdapter` → the five hardware
shims (build-check only) → composition root wiring in `src/main.cpp`.

## 9. Out of scope

- The on-device MQTT broker (`PicoMQTT`), topic routing/decoding, and the
  `OPC_SW_REQ`↔turnout translation layer — a separate future sub-project
  that consumes the WiFi credentials this sub-project produces to bring
  the network up, then starts the broker on top of it.
- mDNS advertisement (`MDNS.begin("loco2mqtt")`) for client discovery —
  belongs with that next sub-project, since it's part of bringing the
  network/broker stack up, not part of commissioning.
- Multi-device identity, presence, or collision detection — not
  applicable to a single-adapter project.
- A setup-mode timeout for the open AP — it stays open indefinitely once
  triggered, until the form is submitted (which reboots). Physical access
  to the device is already required to trigger it, which is the same
  accepted trade-off `MaltbeeController` makes.
- Any config fields beyond WiFi SSID/password (topic prefix, retained
  behavior, broker auth, etc.) — deliberately deferred; Section 2's design
  is built so adding them later is a small, mechanical change, not a
  reason to build them speculatively now.
