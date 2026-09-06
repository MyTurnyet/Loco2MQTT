# Integrating with Loco2MQTT

Context for building a *different* ESP32 device that talks to Loco2MQTT —
a throttle, a fascia control panel, a signal controller, a home-automation
integration, anything that needs to read or command something on the
layout's LocoNet bus without touching LocoNet directly.

Paste this whole file into a new project's context (a `CLAUDE.md`, a
prompt, whatever your workflow uses) when you start building a companion
device. It's a contract document, not a tutorial — it describes what
Loco2MQTT actually does today, not what the layout might eventually do.

## The mental model

Loco2MQTT is not a message queue you write to directly — it's a mirror and
a remote control for LocoNet. **LocoNet is the source of truth.** Publishing
to a `set` topic is a *request*; the corresponding `state` topic only
changes when Loco2MQTT actually sees that change happen on the bus
(whether it caused it or a throttle across the room did). Don't assume a
`set` publish succeeded just because you sent it — if you need
confirmation, watch `state` for the value you asked for.

## Finding the broker

Loco2MQTT runs its own MQTT broker on-device — there is no separate broker
anywhere on the network, and nothing else needs to be installed.

- **Host:** the Loco2MQTT board's IP address on your network.
- **Port:** `1883` (the MQTT default; PicoMQTT doesn't currently expose a
  way to change this).
- **Auth:** none. No username, no password, no TLS.

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

## The turnout contract

This is the only device type Loco2MQTT bridges today. If you're building
something that needs sensor, transponder, or other LocoNet traffic, skip
to "What's not here yet" below — don't build against a contract that
doesn't exist.

| Topic | Direction | Payload | Notes |
|---|---|---|---|
| `loconet/turnout/<address>/state` | Loco2MQTT → you | `CLOSED` or `THROWN` | Plain text, exact case, no JSON. Published on every real change **and** re-published for every known turnout every 30 seconds regardless of change (see "No retained messages" below). |
| `loconet/turnout/<address>/set` | you → Loco2MQTT | `CLOSED` or `THROWN` | Anything else (wrong case, extra whitespace, an unrecognized word) is silently dropped — no error, no response. |

- `<address>` is a plain integer, the same LocoNet turnout number you'd
  enter in JMRI or on a Digitrax throttle — no offset, no remapping.
  Valid range is `1`–`2048`.
- If you only care about turnouts, subscribe to `loconet/turnout/+/state`
  rather than `loconet/+/+/state` — the topic namespace is
  `loconet/<deviceType>/<address>/<topic>`, and a future device type will
  start publishing under the same broker without warning.

## Things this broker can't do (design around these, don't wait for them)

- **QoS is always 0.** PicoMQTT's broker mode doesn't support QoS 1 or 2,
  even if your client requests it. A message can be lost in transit with
  no retry. Don't build anything where a single missed message is a
  problem — see the next point for why that's usually fine anyway.
- **No retained messages.** The broker ignores the MQTT retained flag
  entirely, even though `state` messages are published with it set (for
  whenever the broker situation changes). This is *why* every known
  turnout's state gets re-broadcast every 30 seconds — it's Loco2MQTT's
  own workaround, not a feature you can rely on for anything faster.
  **If your companion device subscribes and needs to know current state
  immediately, it may have to wait up to 30 seconds** for the first
  authoritative value if nothing happens to change in the meantime. Show
  a genuine "unknown" state in your own UI until you've received at least
  one message for an address, rather than assuming a default.
- **The inbound command queue holds 32 entries, drop-oldest.** Don't burst
  a large number of `set` commands at once — space them out if you're
  doing something like "set 20 turnouts for a route" in one go.
- **No authentication, no encryption.** Anything on the same WiFi network
  can publish or subscribe to anything. This is a deliberate choice for a
  trusted home-layout network, not an oversight — but it means your
  companion device doesn't need any credential-handling code for the MQTT
  connection itself, and it also means you shouldn't put this network
  anywhere untrusted.

## What's not here yet

- **Only `turnout` exists as a device type.** Sensors, transponders, block
  occupancy, signals — none of it is bridged to MQTT yet. LocoNet traffic
  for those is currently just logged to Loco2MQTT's own serial console,
  not published anywhere your companion device can reach.
- **No DNS-SD/service-record advertisement** (`_mqtt._tcp.local`) for
  scan-based discovery — see "Finding the broker" above for what mDNS
  support exists today (hostname resolution only).
- **No command acknowledgment.** A `set` either results in a `state`
  change you can observe, or it silently didn't happen (malformed
  input, address out of range, or the physical turnout just didn't move).
  There's no error topic.

If your companion device needs one of these, that's new work on the
Loco2MQTT side, not something you can configure your way into today —
flag it rather than building a workaround that assumes it exists.

## Suggested settings checklist for a new companion device

When you scaffold a new device's config (whatever form that takes —
compile-time constants, its own commissioning flow, a settings file),
make sure these are each a named, changeable value rather than buried
inline in code:

- [ ] Broker host/IP (see above — the one most likely to change)
- [ ] Broker port (`1883` today, but name it rather than hardcode `1883`
      inline everywhere)
- [ ] The specific turnout address(es) this device cares about
- [ ] Reconnect/retry backoff for the MQTT connection (the broker can
      restart whenever Loco2MQTT reboots — your device should recover on
      its own, not require a manual restart)

## Notes of interest

- Loco2MQTT itself is built on an ESP32 running PlatformIO/Arduino and
  uses [PicoMQTT](https://github.com/mlesniew/PicoMQTT) — the same
  library has a client mode, which may be a natural fit if your companion
  device is also an ESP32 and you'd rather not pull in a second MQTT
  library. Not a requirement, just a fit worth knowing about.
- The turnout on/off pulse timing (250ms), the state re-publish interval
  (30s), and the receive-queue caps are all internal to Loco2MQTT and
  don't need to be replicated or matched on your side — they only affect
  *when* you see a `state` update, not anything you need to configure.
- Loco2MQTT's own full technical reference — architecture, the LocoNet
  byte-level protocol details, engineering constraints — lives in this
  project's `README.md` and `CLAUDE.md` if you need to go deeper than
  this contract, e.g. to understand exactly how a `state` value gets
  decoded from the LocoNet bus.
