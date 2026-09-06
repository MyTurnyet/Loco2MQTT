# mDNS Discovery — Design

## Overview

Loco2MQTT's MQTT broker has no discovery mechanism today — a companion
device (or a person with an MQTT client) has to find the board's IP via
their router's connected-devices list and set a DHCP reservation to keep
it stable. This was a known gap called out in
`docs/companion-device-integration.md`. This sub-project closes it by
advertising an mDNS hostname, `loco2mqtt.local`, once the device is on
WiFi — any mDNS-aware OS or library resolves that name to the board's
current IP automatically.

## Scope

**In scope:** hostname resolution only (`loco2mqtt.local` → current IP).

**Explicitly out of scope:**
- DNS-SD / service-record advertisement (`_mqtt._tcp.local`) for
  scan-based discovery — a companion device still needs to be configured
  with the hostname, same as it would be with an IP today; it just stops
  needing to be re-configured when the IP changes.
- A configurable hostname — `loco2mqtt` is a fixed constant, matching the
  existing `Loco2MQTT-Setup` AP name convention. Revisit only if a second
  bridge ever needs to coexist on the same network.
- Handling a WiFi disconnect/reconnect cycle as a special case for mDNS.
  `EspMdnsPort::begin()` fires once, at the same "first real connection"
  trigger point already used for `mqttPort->begin()`. If real hardware
  testing shows the mDNS responder needs to be restarted after a WiFi
  drop, that will be a concrete, reproducible bug to fix — not a
  hypothetical to design around now.

## Component

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

Matches `EspWifiPort`'s existing shape from the MQTT turnout bridge
feature exactly: `#ifdef ARDUINO`-guarded, no port interface (nothing
else in this codebase needs to be decoupled from mDNS status — there is
no meaningful fake for "the network now resolves a hostname"), build-
check-only via `pio run -e esp32dev`, no native test. `ESPmDNS` is
already bundled with the installed ESP32 Arduino framework
(`~/.platformio/packages/framework-arduinoespressif32/libraries/ESPmDNS`)
— no new `platformio.ini` dependency.

## Composition root wiring

`src/main.cpp` gains one `std::optional<EspMdnsPort> mdnsPort;` alongside
the other Normal-mode-only optionals (`wifiPort`, `mqttPort`, etc.),
emplaced in `setupMqttBridge()` next to them, plus one
`bool mdnsBegun = false;` global next to the existing `mqttBegun`.

In `loop()`'s Normal-mode tail, right alongside the existing `mqttBegun`
check:

```cpp
if (!mdnsBegun && wifiPort->isConnected())
{
    mdnsPort->begin("loco2mqtt");
    mdnsBegun = true;
}
```

Kept as its own independent flag rather than folded into the existing
`mqttBegun` check — mDNS and the MQTT broker are separate concerns that
happen to share a trigger condition today, not one concern with two
side effects.

Once running, the device answers as `loco2mqtt.local`, so a companion
device (or a manual MQTT client) can use that hostname instead of a raw
IP. This makes `docs/companion-device-integration.md`'s "Finding the
broker" section stale — the plan's final task updates it. (The published
user-handbook artifact's "How do I find the board's IP address?" FAQ
entry has the same content and should be refreshed too, but that's a
separate published page outside this repo's task workflow — not part of
the plan, a manual follow-up after this ships.)

## Testing

Build-check only (`pio run -e esp32dev`), matching `EspWifiPort`'s own
precedent — no native test, since `EspMdnsPort::begin()` has no logic to
verify, only a single call to a vendor API.
