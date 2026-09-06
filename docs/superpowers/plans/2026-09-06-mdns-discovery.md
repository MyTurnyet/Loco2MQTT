# mDNS Discovery Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Advertise the Loco2MQTT board as `loco2mqtt.local` over mDNS once it's on WiFi, so companion devices and MQTT clients can find the broker without hunting for its IP via the router.

**Architecture:** A single thin adapter, `EspMdnsPort`, wraps the ESP32 Arduino framework's bundled `ESPmDNS` library behind a one-method `begin(hostname)` call — no port interface, matching `EspWifiPort`'s existing shape exactly, since nothing else in the codebase needs to be decoupled from mDNS status. The composition root (`src/main.cpp`) constructs it alongside the other Normal-mode-only adapters and calls `begin("loco2mqtt")` once, at the same "first real WiFi connection" trigger point already used for `mqttPort->begin()`.

**Tech Stack:** ESP32 Arduino framework's bundled `ESPmDNS` library (already present at `~/.platformio/packages/framework-arduinoespressif32/libraries/ESPmDNS` — no new `platformio.ini` dependency).

## Global Constraints

- **In scope:** hostname resolution only (`loco2mqtt.local` → current IP).
- **Out of scope:** DNS-SD/service-record advertisement (`_mqtt._tcp.local`); a configurable hostname (`loco2mqtt` is a fixed constant); any special handling of a WiFi disconnect/reconnect cycle.
- `EspMdnsPort` is `#ifdef ARDUINO`-guarded, has no port interface, and is build-check-only via `pio run -e esp32dev` — no native test, matching `EspWifiPort`'s precedent (there is no meaningful fake for "the network now resolves a hostname").
- The hostname passed to `begin()` is the literal `"loco2mqtt"` (no `.local` suffix — `MDNS.begin()` adds that itself).
- `mdnsBegun` is its own independent `bool` flag, not folded into the existing `mqttBegun` check — mDNS and the MQTT broker are separate concerns that happen to share a trigger condition today.
- This plan's final task updates `docs/companion-device-integration.md`'s "Finding the broker" section, which the spec identifies as made stale by this change.

---

### Task 1: EspMdnsPort adapter

**Files:**
- Create: `lib/Loco2MqttCore/src/adapters/EspMdnsPort.h`
- Create: `lib/Loco2MqttCore/src/adapters/EspMdnsPort.cpp`

**Interfaces:**
- Produces: `class EspMdnsPort { public: void begin(const std::string& hostname); };` — Task 2 constructs one as `std::optional<EspMdnsPort> mdnsPort;` and calls `mdnsPort->begin("loco2mqtt");`.

This adapter has no testable logic of its own (a single call to a vendor API), so there is no native unit test — it is verified by a native compile of the header (which must succeed since native code will `#include` adjacent adapter headers indirectly through the library search path) and a full ESP32 build check, matching how `EspWifiPort` and `EspDigitalPin` were verified in this codebase.

- [ ] **Step 1: Create the header**

```cpp
// lib/Loco2MqttCore/src/adapters/EspMdnsPort.h
#pragma once

#ifdef ARDUINO

#include <string>

class EspMdnsPort
{
public:
    void begin(const std::string& hostname);
};

#endif
```

- [ ] **Step 2: Create the implementation**

```cpp
// lib/Loco2MqttCore/src/adapters/EspMdnsPort.cpp
#ifdef ARDUINO

#include "adapters/EspMdnsPort.h"

#include <ESPmDNS.h>

void EspMdnsPort::begin(const std::string& hostname)
{
    MDNS.begin(hostname.c_str());
}

#endif
```

- [ ] **Step 3: Run the native test suite to confirm nothing broke**

Run: `pio test -e native`
Expected: all existing suites still pass (this task adds no native-visible code, since both new files are `#ifdef ARDUINO`-guarded to nothing under `native`).

- [ ] **Step 4: Build-check against the ESP32 target**

Run: `pio run -e esp32dev`
Expected: `SUCCESS` — this compiles `EspMdnsPort.cpp` for the first time (nothing in `src/main.cpp` references it yet, so this only proves the adapter itself compiles and links against `ESPmDNS`).

- [ ] **Step 5: Commit**

```bash
git add lib/Loco2MqttCore/src/adapters/EspMdnsPort.h lib/Loco2MqttCore/src/adapters/EspMdnsPort.cpp
git commit -m "feat: add EspMdnsPort adapter, build-checked against esp32dev"
```

---

### Task 2: Wire mDNS into the composition root and update integration docs

**Files:**
- Modify: `src/main.cpp`
- Modify: `docs/companion-device-integration.md`

**Interfaces:**
- Consumes: `EspMdnsPort` from Task 1 (`#include "adapters/EspMdnsPort.h"`, `void begin(const std::string& hostname)`).
- Consumes: the existing `EspWifiPort::isConnected() const` (already used by the `mqttBegun` check at `src/main.cpp:161`).

This task has no native test of its own (`src/main.cpp` is the composition root, excluded from the native build per `test_build_src = false` in `platformio.ini`) — it is verified by a full ESP32 build check plus a manual read-through, matching how prior composition-root changes in this codebase (e.g. wiring `PicoMqttPort` in) were verified.

- [ ] **Step 1: Add the `EspMdnsPort` include**

In `src/main.cpp`, add the include in alphabetical order among the existing `adapters/` includes:

```cpp
#include "adapters/EspDigitalInput.h"
#include "adapters/EspMdnsPort.h"
#include "adapters/EspRebootTrigger.h"
```

(This inserts `#include "adapters/EspMdnsPort.h"` between the existing `EspDigitalInput.h` and `EspRebootTrigger.h` lines.)

- [ ] **Step 2: Declare the `mdnsPort` optional and `mdnsBegun` flag**

In `src/main.cpp`, change:

```cpp
std::optional<EspWifiPort> wifiPort;
std::optional<PicoMqttPort> mqttPort;
std::optional<PendingLocoNetSendScheduler> sendScheduler;
std::optional<TurnoutLocoNetDecoder> turnoutLocoNetDecoder;
std::optional<TurnoutMqttEncoder> turnoutMqttEncoder;
std::optional<TurnoutMqttCommandDecoder> turnoutMqttCommandDecoder;
std::optional<TurnoutLocoNetEncoder> turnoutLocoNetEncoder;
std::optional<LocoNetMessageRouter> locoNetMessageRouter;
std::optional<MqttCommandRouter> mqttCommandRouter;
bool mqttBegun = false;
```

to:

```cpp
std::optional<EspWifiPort> wifiPort;
std::optional<PicoMqttPort> mqttPort;
std::optional<EspMdnsPort> mdnsPort;
std::optional<PendingLocoNetSendScheduler> sendScheduler;
std::optional<TurnoutLocoNetDecoder> turnoutLocoNetDecoder;
std::optional<TurnoutMqttEncoder> turnoutMqttEncoder;
std::optional<TurnoutMqttCommandDecoder> turnoutMqttCommandDecoder;
std::optional<TurnoutLocoNetEncoder> turnoutLocoNetEncoder;
std::optional<LocoNetMessageRouter> locoNetMessageRouter;
std::optional<MqttCommandRouter> mqttCommandRouter;
bool mqttBegun = false;
bool mdnsBegun = false;
```

- [ ] **Step 3: Construct `mdnsPort` in `setupMqttBridge()`**

In `src/main.cpp`, change:

```cpp
    void setupMqttBridge(const LocoNetAdapterConfig& config)
    {
        wifiPort.emplace(config.wifiSsid(), config.wifiPassword());
        mqttPort.emplace();
        sendScheduler.emplace(*locoNetPort, systemClock);
```

to:

```cpp
    void setupMqttBridge(const LocoNetAdapterConfig& config)
    {
        wifiPort.emplace(config.wifiSsid(), config.wifiPassword());
        mqttPort.emplace();
        mdnsPort.emplace();
        sendScheduler.emplace(*locoNetPort, systemClock);
```

- [ ] **Step 4: Call `mdnsPort->begin("loco2mqtt")` once WiFi connects, in `loop()`**

In `src/main.cpp`, change:

```cpp
    wifiPort->update();
    if (!mqttBegun && wifiPort->isConnected())
    {
        mqttPort->begin();
        mqttBegun = true;
    }
    mqttPort->update();
```

to:

```cpp
    wifiPort->update();
    if (!mqttBegun && wifiPort->isConnected())
    {
        mqttPort->begin();
        mqttBegun = true;
    }
    if (!mdnsBegun && wifiPort->isConnected())
    {
        mdnsPort->begin("loco2mqtt");
        mdnsBegun = true;
    }
    mqttPort->update();
```

- [ ] **Step 5: Run the native test suite to confirm nothing broke**

Run: `pio test -e native`
Expected: all existing suites still pass (`src/main.cpp` is never part of the native build per `test_build_src = false`, so this change is invisible to `native` — this step only guards against an unrelated regression).

- [ ] **Step 6: Build-check against the ESP32 target**

Run: `pio run -e esp32dev`
Expected: `SUCCESS` — this is the first build that actually exercises `EspMdnsPort` from the composition root.

- [ ] **Step 7: Update `docs/companion-device-integration.md`'s "Finding the broker" section**

In `docs/companion-device-integration.md`, change:

```markdown
**There is no discovery mechanism yet** — no mDNS, no hostname
advertisement, no broadcast. Find the board's IP once via your router's
connected-devices list, then set a DHCP reservation for it so the address
doesn't change later. Because of this:

> **Make the broker host/IP a configurable setting in your companion
> device, not a hardcoded constant.** It's the one thing about this
> integration most likely to need changing after you've already shipped a
> companion device — whether because you re-flash Loco2MQTT, your router
> reassigns addresses, or you eventually add a second bridge. Whatever your
> companion device's own commissioning story is (a config file, its own
> serial commands, a captive portal — mirror whatever pattern you're
> already using for WiFi credentials), the broker address should go
> through it too.
```

to:

```markdown
**The board advertises itself over mDNS as `loco2mqtt.local`** once it's on
WiFi — any mDNS-aware OS or library (including `WiFi.hostByName()` on
another ESP32, or `ping loco2mqtt.local` from a laptop) resolves that name
to the board's current IP automatically. This is hostname resolution only:
there's no `_mqtt._tcp.local` service-record advertisement for scan-based
discovery, so your companion device still needs to be configured with the
hostname — it just never needs to be re-configured again after the board's
IP changes. If your platform or library can't do mDNS lookups, fall back to
finding the board's IP once via your router's connected-devices list and
setting a DHCP reservation for it.

> **Make the broker host a configurable setting in your companion device,
> not a hardcoded constant** — whether you point it at `loco2mqtt.local` or
> a raw IP. This is the one thing about this integration most likely to
> need changing after you've already shipped a companion device — whether
> because you re-flash Loco2MQTT with a different hostname, your mDNS
> resolution isn't available on some network, or you eventually add a
> second bridge. Whatever your companion device's own commissioning story
> is (a config file, its own serial commands, a captive portal — mirror
> whatever pattern you're already using for WiFi credentials), the broker
> address should go through it too.
```

Also, in the same file's "What's not here yet" section, change:

```markdown
- **No mDNS/hostname advertisement** (see "Finding the broker" above).
```

to:

```markdown
- **No DNS-SD/service-record advertisement** (`_mqtt._tcp.local`) for
  scan-based discovery — see "Finding the broker" above for what mDNS
  support exists today (hostname resolution only).
```

- [ ] **Step 8: Commit**

```bash
git add src/main.cpp docs/companion-device-integration.md
git commit -m "feat: advertise loco2mqtt.local via mDNS once WiFi connects

Wires EspMdnsPort into the composition root alongside the existing
mqttBegun trigger, and updates the companion-device integration guide's
'Finding the broker' section to describe hostname resolution instead of
'no discovery mechanism yet'."
```

---

## Self-Review Notes

- **Spec coverage:** "Component" (Task 1), "Composition root wiring" (Task 2, Steps 1-6), the plan's final task updating `docs/companion-device-integration.md` (Task 2, Step 7) are all covered. "Testing" (build-check only, no native test) is reflected in both tasks' verification steps.
- **Explicitly out-of-scope items** (DNS-SD, configurable hostname, WiFi-reconnect special-casing) are not implemented anywhere in this plan — confirmed by re-reading Tasks 1-2.
- **Type consistency:** `EspMdnsPort::begin(const std::string&)` is declared identically in Task 1 and called identically (`mdnsPort->begin("loco2mqtt")`) in Task 2.
- The manual, non-plan follow-up noted in the spec (refreshing the published HTML handbook artifact's FAQ entry) is intentionally not a task here — it's a separate published page outside this repo's task workflow.
