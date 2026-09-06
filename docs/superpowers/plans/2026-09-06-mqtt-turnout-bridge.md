# MQTT Turnout Bridge Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Bridge Digitrax LocoNet turnout traffic to and from MQTT on a real
ESP32, with WiFi connecting from stored commissioning credentials and
PicoMQTT running on-device as the broker.

**Architecture:** Two mirrored pipelines (LocoNet→MQTT and MQTT→LocoNet)
built from small per-device-type decoders/encoders dispatched by two
generic routers, all pure logic and natively testable; only WiFi and the
PicoMQTT broker itself are genuinely hardware-bound adapters. See
`docs/superpowers/specs/2026-09-06-mqtt-turnout-bridge-design.md` for the
full design and citations — this plan implements it exactly.

**Tech Stack:** C++17, PlatformIO (`native` + `esp32dev` envs), Catch2,
`LocoNetESP32HB` (existing dependency), PicoMQTT (new dependency).

## Global Constraints

- **TDD, no mocking frameworks.** Every test uses a real object or a
  hand-written fake in `test/support/` — never a mock.
- **Methods ≤ 8 lines, cognitive complexity < 4**, except pure one-line-per-
  branch dispatch chains (established exemption from the commissioning
  plan) — several router/decoder dispatch methods here are exactly that
  shape and are not to be split further for line count alone.
- **Constructor injection everywhere.** No statics or globals outside
  `src/main.cpp`.
- **Immutable value objects, header-only.** `TurnoutAddress`,
  `TurnoutPosition`, `TurnoutStateChanged`, `SetTurnoutPosition`,
  `MqttMessage`, `IncomingMqttMessage`, and `PendingLocoNetSend` are all
  trivial data holders with every method defined inline in their `.h` —
  no `.cpp` file, no setters.
- **Native-link rule** (established in the prior commissioning plan): a
  test file that names or constructs a class with an out-of-line (`.cpp`)
  method must `#include` that class's own header directly — never rely on
  reaching it transitively through a fake's include chain, or the native
  build can fail to *link* (not compile). This applies to every class in
  this plan that has a `.cpp`: `PendingLocoNetSendScheduler`,
  `LocoNetMessageRouter`, `MqttCommandRouter`, `TurnoutLocoNetDecoder`,
  `TurnoutMqttEncoder`, `TurnoutMqttCommandDecoder`,
  `TurnoutLocoNetEncoder`, `EspWifiPort`, `PicoMqttPort`. It does **not**
  apply to the header-only value objects above, or to any port interface
  (pure virtual, never has a `.cpp`).
- **`native`'s `-Ilib/Loco2MqttCore/src` already covers the new `turnout/`
  subfolder** — no `platformio.ini` change needed for it.
- **Two-tier testing.** Everything under `lib/Loco2MqttCore/src/{domain,
  ports,application,turnout}` is native-tested (`pio test -e native`).
  Only `lib/Loco2MqttCore/src/adapters/EspWifiPort.*` and
  `PicoMqttPort.*` are build-check-only (`pio run -e esp32dev`), matching
  `LocoNetEsp32Port`'s existing precedent — no native test for them.
- **MQTT contract:** `loconet/turnout/<address>/state` (payload exactly
  `"CLOSED"` or `"THROWN"`, `MqttMessage::retained()` set `true` in code
  though PicoMQTT's broker mode does not honor it — see next point) and
  `loconet/turnout/<address>/set` (same payload values, not retained).
- **PicoMQTT reality, confirmed against its own README
  ([mlesniew/PicoMQTT](https://github.com/mlesniew/PicoMQTT)):** broker
  mode is QoS 0 only and ignores retained messages and will entirely.
  `PicoMQTT::Server mqtt;` (no-arg construction), `mqtt.begin()` once after
  WiFi is up, `mqtt.publish(topic, payload)`, `mqtt.subscribe(pattern,
  callback)` registered once, `mqtt.loop()` pumped from `update()`.
- **`kOffPulseDelayMs = 250`** — the on-pulse → off-pulse gap. A device-
  timing choice under our control, not a wire-protocol fact; verify
  against real turnout hardware, adjust if needed.
- **`kStateRepublishIntervalMs = 30000`** — how often
  `LocoNetMessageRouter` re-publishes every decoder's full known state,
  the workaround for PicoMQTT ignoring retained messages (see spec's "MQTT
  contract" section for why this exists).
- **LocoNet checksum.** `LocoNetESP32HB`'s TX path
  (`lnWriteMsg`→`hybrid_write`) never computes one — confirmed by reading
  `.pio/libdeps/esp32dev/ESPLocoNetHybridESP32/src/IoTT_LocoNetHBESP32.cpp`
  directly. `TurnoutLocoNetEncoder` must compute and append it itself via
  `computeLocoNetChecksum()`.
- **Protocol bytes, cited from JMRI's `LnConstants.java`**
  ([JMRI/JMRI on GitHub](https://github.com/JMRI/JMRI/blob/master/java/src/jmri/jmrix/loconet/LnConstants.java)):
  `OPC_SW_REQ = 0xB0`, `OPC_SW_REP = 0xB1`, `OPC_SW_REQ_DIR = 0x20` (set =
  Closed), `OPC_SW_REQ_OUT = 0x10` (set = output on),
  `OPC_SW_REP_INPUTS = 0x40` (set = sensor/input report, not a commanded-
  position report), `OPC_SW_REP_CLOSED = 0x20` (meaningful only when
  `OPC_SW_REP_INPUTS` is clear; set = Closed). Full byte layout is spelled
  out in Task 9 and Task 12 below — copy it verbatim, it is not to be
  re-derived from memory.
- **New pinned dependency:** add to `platformio.ini`'s `[env:esp32dev]`
  `lib_deps`, matching the existing pinning convention:
  `https://github.com/mlesniew/PicoMQTT.git#8518da5fd5fa3d1c147150f76957a0e3ecf40ab4`.
- **BootMode boundary.** The entire MQTT bridge (WiFi, PicoMQTT, both
  routers, the turnout classes, the send scheduler) is constructed only
  when `selectBootMode(...)` returns `BootMode::Normal`.
  `NeedsCommissioning` and `WirelessSetup` behavior is unchanged from the
  already-shipped commissioning sub-project.
- **`TurnoutAddress` performs no runtime validation.** Every call site
  that constructs one already guarantees range 1..2048 structurally (see
  Task 1). `TurnoutMqttCommandDecoder` validates the untrusted MQTT
  address string itself and returns `std::nullopt` on failure (Task 11) —
  this project's own principle is to validate only at system boundaries.

---

### Task 1: TurnoutAddress

**Files:**
- Create: `lib/Loco2MqttCore/src/domain/TurnoutAddress.h`
- Test: `test/test_turnout_address/test_main.cpp`

**Interfaces:**
- Produces: `TurnoutAddress(int address)`, `int value() const`.

- [ ] **Step 1: Write the failing test**

```cpp
// test/test_turnout_address/test_main.cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/TurnoutAddress.h"

TEST_CASE("TurnoutAddress stores and returns its value")
{
    TurnoutAddress address(5);

    REQUIRE(address.value() == 5);
}

TEST_CASE("TurnoutAddress supports the low end of the LocoNet range")
{
    TurnoutAddress address(1);

    REQUIRE(address.value() == 1);
}

TEST_CASE("TurnoutAddress supports the high end of the LocoNet range")
{
    TurnoutAddress address(2048);

    REQUIRE(address.value() == 2048);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `pio test -e native -f test_turnout_address`
Expected: FAIL — `fatal error: domain/TurnoutAddress.h: No such file or directory`

- [ ] **Step 3: Write minimal implementation**

```cpp
// lib/Loco2MqttCore/src/domain/TurnoutAddress.h
#pragma once

class TurnoutAddress
{
public:
    explicit TurnoutAddress(int address) : address_(address)
    {
    }

    int value() const
    {
        return address_;
    }

private:
    int address_;
};
```

- [ ] **Step 4: Run test to verify it passes**

Run: `pio test -e native -f test_turnout_address`
Expected: PASS — 3 test cases, 3 assertions

- [ ] **Step 5: Commit**

```bash
git add lib/Loco2MqttCore/src/domain/TurnoutAddress.h test/test_turnout_address/test_main.cpp
git commit -m "feat: add TurnoutAddress domain value object"
```

---

### Task 2: TurnoutPosition, TurnoutStateChanged, SetTurnoutPosition, DomainEvent, DomainCommand

**Files:**
- Create: `lib/Loco2MqttCore/src/domain/TurnoutPosition.h`
- Create: `lib/Loco2MqttCore/src/domain/TurnoutStateChanged.h`
- Create: `lib/Loco2MqttCore/src/domain/SetTurnoutPosition.h`
- Create: `lib/Loco2MqttCore/src/domain/DomainEvent.h`
- Create: `lib/Loco2MqttCore/src/domain/DomainCommand.h`
- Test: `test/test_turnout_domain_events/test_main.cpp`

**Interfaces:**
- Consumes: `TurnoutAddress` from Task 1.
- Produces: `enum class TurnoutPosition { Closed, Thrown }`;
  `TurnoutStateChanged(TurnoutAddress, TurnoutPosition)` with
  `.address()`/`.position()`; `SetTurnoutPosition(TurnoutAddress,
  TurnoutPosition)` with the same accessors; `using DomainEvent =
  std::variant<TurnoutStateChanged>`; `using DomainCommand =
  std::variant<SetTurnoutPosition>`.

- [ ] **Step 1: Write the failing test**

```cpp
// test/test_turnout_domain_events/test_main.cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include <variant>

#include "domain/DomainCommand.h"
#include "domain/DomainEvent.h"
#include "domain/SetTurnoutPosition.h"
#include "domain/TurnoutAddress.h"
#include "domain/TurnoutPosition.h"
#include "domain/TurnoutStateChanged.h"

TEST_CASE("TurnoutStateChanged stores address and position")
{
    TurnoutStateChanged event(TurnoutAddress(5), TurnoutPosition::Closed);

    REQUIRE(event.address().value() == 5);
    REQUIRE(event.position() == TurnoutPosition::Closed);
}

TEST_CASE("SetTurnoutPosition stores address and position")
{
    SetTurnoutPosition command(TurnoutAddress(7), TurnoutPosition::Thrown);

    REQUIRE(command.address().value() == 7);
    REQUIRE(command.position() == TurnoutPosition::Thrown);
}

TEST_CASE("DomainEvent holds a TurnoutStateChanged")
{
    DomainEvent event = TurnoutStateChanged(TurnoutAddress(5), TurnoutPosition::Closed);

    REQUIRE(std::get<TurnoutStateChanged>(event).address().value() == 5);
}

TEST_CASE("DomainCommand holds a SetTurnoutPosition")
{
    DomainCommand command = SetTurnoutPosition(TurnoutAddress(7), TurnoutPosition::Thrown);

    REQUIRE(std::get<SetTurnoutPosition>(command).address().value() == 7);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `pio test -e native -f test_turnout_domain_events`
Expected: FAIL — `fatal error: domain/TurnoutPosition.h: No such file or directory`

- [ ] **Step 3: Write minimal implementation**

```cpp
// lib/Loco2MqttCore/src/domain/TurnoutPosition.h
#pragma once

enum class TurnoutPosition
{
    Closed,
    Thrown
};
```

```cpp
// lib/Loco2MqttCore/src/domain/TurnoutStateChanged.h
#pragma once

#include "domain/TurnoutAddress.h"
#include "domain/TurnoutPosition.h"

class TurnoutStateChanged
{
public:
    TurnoutStateChanged(TurnoutAddress address, TurnoutPosition position)
        : address_(address), position_(position)
    {
    }

    TurnoutAddress address() const
    {
        return address_;
    }

    TurnoutPosition position() const
    {
        return position_;
    }

private:
    TurnoutAddress address_;
    TurnoutPosition position_;
};
```

```cpp
// lib/Loco2MqttCore/src/domain/SetTurnoutPosition.h
#pragma once

#include "domain/TurnoutAddress.h"
#include "domain/TurnoutPosition.h"

class SetTurnoutPosition
{
public:
    SetTurnoutPosition(TurnoutAddress address, TurnoutPosition position)
        : address_(address), position_(position)
    {
    }

    TurnoutAddress address() const
    {
        return address_;
    }

    TurnoutPosition position() const
    {
        return position_;
    }

private:
    TurnoutAddress address_;
    TurnoutPosition position_;
};
```

```cpp
// lib/Loco2MqttCore/src/domain/DomainEvent.h
#pragma once

#include <variant>

#include "domain/TurnoutStateChanged.h"

using DomainEvent = std::variant<TurnoutStateChanged>;
```

```cpp
// lib/Loco2MqttCore/src/domain/DomainCommand.h
#pragma once

#include <variant>

#include "domain/SetTurnoutPosition.h"

using DomainCommand = std::variant<SetTurnoutPosition>;
```

- [ ] **Step 4: Run test to verify it passes**

Run: `pio test -e native -f test_turnout_domain_events`
Expected: PASS — 4 test cases, 4 assertions

- [ ] **Step 5: Commit**

```bash
git add lib/Loco2MqttCore/src/domain/TurnoutPosition.h lib/Loco2MqttCore/src/domain/TurnoutStateChanged.h lib/Loco2MqttCore/src/domain/SetTurnoutPosition.h lib/Loco2MqttCore/src/domain/DomainEvent.h lib/Loco2MqttCore/src/domain/DomainCommand.h test/test_turnout_domain_events/test_main.cpp
git commit -m "feat: add turnout domain events/commands and DomainEvent/DomainCommand variants"
```

---

### Task 3: LocoNet checksum helper

**Files:**
- Create: `lib/Loco2MqttCore/src/domain/LocoNetChecksum.h`
- Test: `test/test_loco_net_checksum/test_main.cpp`

**Interfaces:**
- Produces: `uint8_t computeLocoNetChecksum(const std::vector<uint8_t>& bytesBeforeChecksum)`.

**Why this exists:** `LocoNetESP32HB`'s TX path never computes a checksum
(see Global Constraints) — this is the standard LocoNet XOR checksum:
the byte `X` such that XORing every message byte together, including `X`,
equals `0xFF`.

- [ ] **Step 1: Write the failing test**

```cpp
// test/test_loco_net_checksum/test_main.cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/LocoNetChecksum.h"

TEST_CASE("computeLocoNetChecksum produces a checksum that XORs the whole message to 0xFF")
{
    // 0xB0, 0x04, 0x30 (address 5, Closed, output on) -> checksum 0x7B,
    // hand-verified: 0xB0 ^ 0x04 ^ 0x30 ^ 0x7B == 0xFF
    std::vector<uint8_t> bytes = {0xB0, 0x04, 0x30};

    REQUIRE(computeLocoNetChecksum(bytes) == 0x7B);
}

TEST_CASE("computeLocoNetChecksum handles the off-pulse variant of the same message")
{
    // Same address/direction, output off (0x20 only) -> checksum 0x6B
    std::vector<uint8_t> bytes = {0xB0, 0x04, 0x20};

    REQUIRE(computeLocoNetChecksum(bytes) == 0x6B);
}

TEST_CASE("computeLocoNetChecksum handles the maximum turnout address")
{
    // Address 2048 (zero-based 2047 = 0x7FF), Closed, output on -> checksum 0x0F
    std::vector<uint8_t> bytes = {0xB0, 0x7F, 0x3F};

    REQUIRE(computeLocoNetChecksum(bytes) == 0x0F);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `pio test -e native -f test_loco_net_checksum`
Expected: FAIL — `fatal error: domain/LocoNetChecksum.h: No such file or directory`

- [ ] **Step 3: Write minimal implementation**

```cpp
// lib/Loco2MqttCore/src/domain/LocoNetChecksum.h
#pragma once

#include <cstdint>
#include <vector>

inline uint8_t computeLocoNetChecksum(const std::vector<uint8_t>& bytesBeforeChecksum)
{
    uint8_t xorAccumulator = 0;
    for (uint8_t byte : bytesBeforeChecksum)
    {
        xorAccumulator ^= byte;
    }
    return xorAccumulator ^ 0xFF;
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `pio test -e native -f test_loco_net_checksum`
Expected: PASS — 3 test cases, 3 assertions

- [ ] **Step 5: Commit**

```bash
git add lib/Loco2MqttCore/src/domain/LocoNetChecksum.h test/test_loco_net_checksum/test_main.cpp
git commit -m "feat: add LocoNet XOR checksum helper"
```

---

### Task 4: MqttMessage and IncomingMqttMessage

**Files:**
- Create: `lib/Loco2MqttCore/src/domain/MqttMessage.h`
- Create: `lib/Loco2MqttCore/src/domain/IncomingMqttMessage.h`
- Test: `test/test_mqtt_message/test_main.cpp`

**Interfaces:**
- Produces: `MqttMessage(std::string topic, std::string payload, bool
  retained)` with `.topic()`/`.payload()`/`.retained()`;
  `IncomingMqttMessage(std::string topic, std::string payload)` with
  `.topic()`/`.payload()`.

- [ ] **Step 1: Write the failing test**

```cpp
// test/test_mqtt_message/test_main.cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/IncomingMqttMessage.h"
#include "domain/MqttMessage.h"

TEST_CASE("MqttMessage stores topic, payload, and retained")
{
    MqttMessage message("loconet/turnout/5/state", "CLOSED", true);

    REQUIRE(message.topic() == "loconet/turnout/5/state");
    REQUIRE(message.payload() == "CLOSED");
    REQUIRE(message.retained() == true);
}

TEST_CASE("IncomingMqttMessage stores topic and payload")
{
    IncomingMqttMessage message("loconet/turnout/5/set", "THROWN");

    REQUIRE(message.topic() == "loconet/turnout/5/set");
    REQUIRE(message.payload() == "THROWN");
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `pio test -e native -f test_mqtt_message`
Expected: FAIL — `fatal error: domain/MqttMessage.h: No such file or directory`

- [ ] **Step 3: Write minimal implementation**

```cpp
// lib/Loco2MqttCore/src/domain/MqttMessage.h
#pragma once

#include <string>
#include <utility>

class MqttMessage
{
public:
    MqttMessage(std::string topic, std::string payload, bool retained)
        : topic_(std::move(topic)), payload_(std::move(payload)), retained_(retained)
    {
    }

    const std::string& topic() const
    {
        return topic_;
    }

    const std::string& payload() const
    {
        return payload_;
    }

    bool retained() const
    {
        return retained_;
    }

private:
    std::string topic_;
    std::string payload_;
    bool retained_;
};
```

```cpp
// lib/Loco2MqttCore/src/domain/IncomingMqttMessage.h
#pragma once

#include <string>
#include <utility>

class IncomingMqttMessage
{
public:
    IncomingMqttMessage(std::string topic, std::string payload)
        : topic_(std::move(topic)), payload_(std::move(payload))
    {
    }

    const std::string& topic() const
    {
        return topic_;
    }

    const std::string& payload() const
    {
        return payload_;
    }

private:
    std::string topic_;
    std::string payload_;
};
```

- [ ] **Step 4: Run test to verify it passes**

Run: `pio test -e native -f test_mqtt_message`
Expected: PASS — 2 test cases, 5 assertions

- [ ] **Step 5: Commit**

```bash
git add lib/Loco2MqttCore/src/domain/MqttMessage.h lib/Loco2MqttCore/src/domain/IncomingMqttMessage.h test/test_mqtt_message/test_main.cpp
git commit -m "feat: add MqttMessage and IncomingMqttMessage domain objects"
```

---

### Task 5: MqttPort and FakeMqttPort

**Files:**
- Create: `lib/Loco2MqttCore/src/ports/MqttPort.h`
- Create: `test/support/FakeMqttPort.h`
- Test: `test/test_fake_mqtt_port/test_main.cpp`

**Interfaces:**
- Consumes: `MqttMessage`, `IncomingMqttMessage` from Task 4.
- Produces: `MqttPort` with `virtual void publish(const MqttMessage&) =
  0` and `virtual std::optional<IncomingMqttMessage> receiveCommand() =
  0`; `FakeMqttPort` with `enqueueCommand(const IncomingMqttMessage&)` and
  `const std::vector<MqttMessage>& published() const`.

- [ ] **Step 1: Write the failing test**

```cpp
// test/test_fake_mqtt_port/test_main.cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "support/FakeMqttPort.h"

TEST_CASE("receiveCommand returns nullopt when nothing is queued")
{
    FakeMqttPort port;

    REQUIRE(port.receiveCommand().has_value() == false);
}

TEST_CASE("receiveCommand returns an enqueued command, then nullopt again")
{
    FakeMqttPort port;
    port.enqueueCommand(IncomingMqttMessage("loconet/turnout/5/set", "CLOSED"));

    auto received = port.receiveCommand();

    REQUIRE(received.has_value());
    REQUIRE(received->topic() == "loconet/turnout/5/set");
    REQUIRE(port.receiveCommand().has_value() == false);
}

TEST_CASE("enqueued commands are read back in FIFO order")
{
    FakeMqttPort port;
    port.enqueueCommand(IncomingMqttMessage("a", "1"));
    port.enqueueCommand(IncomingMqttMessage("b", "2"));

    REQUIRE(port.receiveCommand()->topic() == "a");
    REQUIRE(port.receiveCommand()->topic() == "b");
}

TEST_CASE("publish records published messages in order")
{
    FakeMqttPort port;
    port.publish(MqttMessage("loconet/turnout/5/state", "CLOSED", true));
    port.publish(MqttMessage("loconet/turnout/7/state", "THROWN", true));

    REQUIRE(port.published().size() == 2);
    REQUIRE(port.published()[0].topic() == "loconet/turnout/5/state");
    REQUIRE(port.published()[1].topic() == "loconet/turnout/7/state");
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `pio test -e native -f test_fake_mqtt_port`
Expected: FAIL — `fatal error: support/FakeMqttPort.h: No such file or directory`

- [ ] **Step 3: Write minimal implementation**

```cpp
// lib/Loco2MqttCore/src/ports/MqttPort.h
#pragma once

#include <optional>

#include "domain/IncomingMqttMessage.h"
#include "domain/MqttMessage.h"

class MqttPort
{
public:
    virtual ~MqttPort() = default;
    virtual void publish(const MqttMessage& message) = 0;
    virtual std::optional<IncomingMqttMessage> receiveCommand() = 0;
};
```

```cpp
// test/support/FakeMqttPort.h
#pragma once

#include <deque>
#include <vector>

#include "ports/MqttPort.h"

class FakeMqttPort : public MqttPort
{
public:
    void enqueueCommand(const IncomingMqttMessage& message)
    {
        toReceive_.push_back(message);
    }

    std::optional<IncomingMqttMessage> receiveCommand() override
    {
        if (toReceive_.empty())
        {
            return std::nullopt;
        }
        IncomingMqttMessage message = toReceive_.front();
        toReceive_.pop_front();
        return message;
    }

    void publish(const MqttMessage& message) override
    {
        published_.push_back(message);
    }

    const std::vector<MqttMessage>& published() const
    {
        return published_;
    }

private:
    std::deque<IncomingMqttMessage> toReceive_;
    std::vector<MqttMessage> published_;
};
```

- [ ] **Step 4: Run test to verify it passes**

Run: `pio test -e native -f test_fake_mqtt_port`
Expected: PASS — 4 test cases, 8 assertions

- [ ] **Step 5: Commit**

```bash
git add lib/Loco2MqttCore/src/ports/MqttPort.h test/support/FakeMqttPort.h test/test_fake_mqtt_port/test_main.cpp
git commit -m "feat: add MqttPort port and FakeMqttPort"
```

---

### Task 6: PendingLocoNetSend, LocoNetSendScheduler port, FakeLocoNetSendScheduler, PendingLocoNetSendScheduler

**Files:**
- Create: `lib/Loco2MqttCore/src/domain/PendingLocoNetSend.h`
- Create: `lib/Loco2MqttCore/src/ports/LocoNetSendScheduler.h`
- Create: `test/support/FakeLocoNetSendScheduler.h`
- Create: `lib/Loco2MqttCore/src/application/PendingLocoNetSendScheduler.h`
- Create: `lib/Loco2MqttCore/src/application/PendingLocoNetSendScheduler.cpp`
- Test: `test/test_pending_loco_net_send_scheduler/test_main.cpp`

**Interfaces:**
- Consumes: `LocoNetMessage` (existing), `LocoNetPort`/`FakeLocoNetPort`
  (existing), `Clock`/`FakeClock` (existing, `nowMilliseconds()`).
- Produces: `LocoNetSendScheduler` port with `sendNow(const
  LocoNetMessage&)` and `sendAfter(const LocoNetMessage&, unsigned long
  delayMilliseconds)`; `FakeLocoNetSendScheduler` recording both kinds of
  calls (`sentNow()`, `scheduled()` — each returning `const
  std::vector<...>&`); `PendingLocoNetSendScheduler` — the one real
  implementation, plus `void update()` (not part of the port — called
  directly from `main.cpp`, matching `LocoNetEsp32Port::update()`'s
  precedent of a concrete-only pump method).

- [ ] **Step 1: Write the failing test**

```cpp
// test/test_pending_loco_net_send_scheduler/test_main.cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "application/PendingLocoNetSendScheduler.h"
#include "support/FakeClock.h"
#include "support/FakeLocoNetPort.h"

TEST_CASE("sendNow sends immediately")
{
    FakeLocoNetPort port;
    FakeClock clock;
    PendingLocoNetSendScheduler scheduler(port, clock);

    scheduler.sendNow(LocoNetMessage({0xB0, 0x04, 0x30, 0x7B}));

    REQUIRE(port.sent().size() == 1);
}

TEST_CASE("sendAfter does not send before the delay has elapsed")
{
    FakeLocoNetPort port;
    FakeClock clock;
    PendingLocoNetSendScheduler scheduler(port, clock);
    clock.setNowMilliseconds(1000);

    scheduler.sendAfter(LocoNetMessage({0xB0, 0x04, 0x20, 0x6B}), 250);
    clock.setNowMilliseconds(1249);
    scheduler.update();

    REQUIRE(port.sent().size() == 0);
}

TEST_CASE("sendAfter sends once the delay has elapsed")
{
    FakeLocoNetPort port;
    FakeClock clock;
    PendingLocoNetSendScheduler scheduler(port, clock);
    clock.setNowMilliseconds(1000);

    scheduler.sendAfter(LocoNetMessage({0xB0, 0x04, 0x20, 0x6B}), 250);
    clock.setNowMilliseconds(1250);
    scheduler.update();

    REQUIRE(port.sent().size() == 1);
    REQUIRE(port.sent()[0].bytes() == std::vector<uint8_t>{0xB0, 0x04, 0x20, 0x6B});
}

TEST_CASE("a due send is only sent once across repeated update calls")
{
    FakeLocoNetPort port;
    FakeClock clock;
    PendingLocoNetSendScheduler scheduler(port, clock);

    scheduler.sendAfter(LocoNetMessage({0xB0, 0x04, 0x20, 0x6B}), 250);
    clock.setNowMilliseconds(250);
    scheduler.update();
    scheduler.update();

    REQUIRE(port.sent().size() == 1);
}

TEST_CASE("multiple pending sends each fire at their own due time")
{
    FakeLocoNetPort port;
    FakeClock clock;
    PendingLocoNetSendScheduler scheduler(port, clock);

    scheduler.sendAfter(LocoNetMessage({0xB0, 0x04, 0x20, 0x6B}), 100);
    scheduler.sendAfter(LocoNetMessage({0xB0, 0x08, 0x20, 0x67}), 300);
    clock.setNowMilliseconds(100);
    scheduler.update();

    REQUIRE(port.sent().size() == 1);

    clock.setNowMilliseconds(300);
    scheduler.update();

    REQUIRE(port.sent().size() == 2);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `pio test -e native -f test_pending_loco_net_send_scheduler`
Expected: FAIL — `fatal error: application/PendingLocoNetSendScheduler.h: No such file or directory`

- [ ] **Step 3: Write minimal implementation**

```cpp
// lib/Loco2MqttCore/src/domain/PendingLocoNetSend.h
#pragma once

#include <utility>

#include "domain/LocoNetMessage.h"

class PendingLocoNetSend
{
public:
    PendingLocoNetSend(LocoNetMessage message, unsigned long dueAtMilliseconds)
        : message_(std::move(message)), dueAtMilliseconds_(dueAtMilliseconds)
    {
    }

    const LocoNetMessage& message() const
    {
        return message_;
    }

    unsigned long dueAtMilliseconds() const
    {
        return dueAtMilliseconds_;
    }

private:
    LocoNetMessage message_;
    unsigned long dueAtMilliseconds_;
};
```

```cpp
// lib/Loco2MqttCore/src/ports/LocoNetSendScheduler.h
#pragma once

#include "domain/LocoNetMessage.h"

class LocoNetSendScheduler
{
public:
    virtual ~LocoNetSendScheduler() = default;
    virtual void sendNow(const LocoNetMessage& message) = 0;
    virtual void sendAfter(const LocoNetMessage& message, unsigned long delayMilliseconds) = 0;
};
```

```cpp
// test/support/FakeLocoNetSendScheduler.h
#pragma once

#include <utility>
#include <vector>

#include "ports/LocoNetSendScheduler.h"

class FakeLocoNetSendScheduler : public LocoNetSendScheduler
{
public:
    void sendNow(const LocoNetMessage& message) override
    {
        sentNow_.push_back(message);
    }

    void sendAfter(const LocoNetMessage& message, unsigned long delayMilliseconds) override
    {
        scheduled_.emplace_back(message, delayMilliseconds);
    }

    const std::vector<LocoNetMessage>& sentNow() const
    {
        return sentNow_;
    }

    const std::vector<std::pair<LocoNetMessage, unsigned long>>& scheduled() const
    {
        return scheduled_;
    }

private:
    std::vector<LocoNetMessage> sentNow_;
    std::vector<std::pair<LocoNetMessage, unsigned long>> scheduled_;
};
```

```cpp
// lib/Loco2MqttCore/src/application/PendingLocoNetSendScheduler.h
#pragma once

#include <vector>

#include "domain/PendingLocoNetSend.h"
#include "ports/Clock.h"
#include "ports/LocoNetPort.h"
#include "ports/LocoNetSendScheduler.h"

class PendingLocoNetSendScheduler : public LocoNetSendScheduler
{
public:
    PendingLocoNetSendScheduler(LocoNetPort& port, Clock& clock) : port_(port), clock_(clock)
    {
    }

    void sendNow(const LocoNetMessage& message) override;
    void sendAfter(const LocoNetMessage& message, unsigned long delayMilliseconds) override;
    void update();

private:
    LocoNetPort& port_;
    Clock& clock_;
    std::vector<PendingLocoNetSend> pending_;
};
```

```cpp
// lib/Loco2MqttCore/src/application/PendingLocoNetSendScheduler.cpp
#include "application/PendingLocoNetSendScheduler.h"

void PendingLocoNetSendScheduler::sendNow(const LocoNetMessage& message)
{
    port_.send(message);
}

void PendingLocoNetSendScheduler::sendAfter(const LocoNetMessage& message, unsigned long delayMilliseconds)
{
    pending_.emplace_back(message, clock_.nowMilliseconds() + delayMilliseconds);
}

void PendingLocoNetSendScheduler::update()
{
    const unsigned long now = clock_.nowMilliseconds();
    auto it = pending_.begin();
    while (it != pending_.end())
    {
        if (it->dueAtMilliseconds() > now)
        {
            ++it;
            continue;
        }
        port_.send(it->message());
        it = pending_.erase(it);
    }
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `pio test -e native -f test_pending_loco_net_send_scheduler`
Expected: PASS — 5 test cases, 8 assertions

- [ ] **Step 5: Commit**

```bash
git add lib/Loco2MqttCore/src/domain/PendingLocoNetSend.h lib/Loco2MqttCore/src/ports/LocoNetSendScheduler.h test/support/FakeLocoNetSendScheduler.h lib/Loco2MqttCore/src/application/PendingLocoNetSendScheduler.h lib/Loco2MqttCore/src/application/PendingLocoNetSendScheduler.cpp test/test_pending_loco_net_send_scheduler/test_main.cpp
git commit -m "feat: add LocoNetSendScheduler port and PendingLocoNetSendScheduler"
```

---

### Task 7: LocoNetMessageDecoder, MqttEventEncoder ports, and LocoNetMessageRouter

**Files:**
- Create: `lib/Loco2MqttCore/src/ports/LocoNetMessageDecoder.h`
- Create: `lib/Loco2MqttCore/src/ports/MqttEventEncoder.h`
- Create: `lib/Loco2MqttCore/src/application/LocoNetMessageRouter.h`
- Create: `lib/Loco2MqttCore/src/application/LocoNetMessageRouter.cpp`
- Test: `test/test_loco_net_message_router/test_main.cpp`

**Interfaces:**
- Consumes: `DomainEvent` (Task 2), `LocoNetMessage` (existing),
  `MqttMessage` (Task 4), `MqttPort`/`FakeMqttPort` (Task 5),
  `Clock`/`FakeClock` (existing), `LocoNetPort`/`FakeLocoNetPort`
  (existing).
- Produces: `LocoNetMessageDecoder` port — `bool canDecode(uint8_t
  opcode) const`, `std::optional<DomainEvent> decode(const
  LocoNetMessage&)` (non-const — implementations may hold dedup state),
  `std::vector<DomainEvent> allKnownStates() const`. `MqttEventEncoder`
  port — `MqttMessage encode(const DomainEvent&) const`.
  `LocoNetMessageRouter(LocoNetPort&, MqttPort&, Clock&,
  std::vector<std::pair<LocoNetMessageDecoder*, MqttEventEncoder*>>
  decoders)` with `void update()`. `kStateRepublishIntervalMs = 30000` is
  a `static constexpr` on the router.

**Test doubles for this task only** (trivial, defined directly in the test
file — not added to `test/support/`, since they exist purely to prove
router dispatch and are meaningless outside this test, per the design
doc's own suggestion to use "two or three trivial fake decoders"):

- [ ] **Step 1: Write the failing test**

```cpp
// test/test_loco_net_message_router/test_main.cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "application/LocoNetMessageRouter.h"
#include "domain/TurnoutAddress.h"
#include "domain/TurnoutPosition.h"
#include "domain/TurnoutStateChanged.h"
#include "support/FakeClock.h"
#include "support/FakeLocoNetPort.h"
#include "support/FakeMqttPort.h"

namespace
{
    class FakeDecoder : public LocoNetMessageDecoder
    {
    public:
        explicit FakeDecoder(uint8_t opcode) : opcode_(opcode)
        {
        }

        bool canDecode(uint8_t opcode) const override
        {
            return opcode == opcode_;
        }

        std::optional<DomainEvent> decode(const LocoNetMessage&) override
        {
            decodeCallCount_++;
            return nextResult_;
        }

        std::vector<DomainEvent> allKnownStates() const override
        {
            return knownStates_;
        }

        void setNextResult(std::optional<DomainEvent> result)
        {
            nextResult_ = result;
        }

        void setKnownStates(std::vector<DomainEvent> states)
        {
            knownStates_ = states;
        }

        int decodeCallCount() const
        {
            return decodeCallCount_;
        }

    private:
        uint8_t opcode_;
        std::optional<DomainEvent> nextResult_;
        std::vector<DomainEvent> knownStates_;
        int decodeCallCount_ = 0;
    };

    class FakeEncoder : public MqttEventEncoder
    {
    public:
        MqttMessage encode(const DomainEvent&) const override
        {
            return MqttMessage("fake/topic", "fake-payload", false);
        }
    };
}

TEST_CASE("dispatches a decoded event to the matching encoder and publishes it")
{
    FakeLocoNetPort locoNetPort;
    FakeMqttPort mqttPort;
    FakeClock clock;
    FakeDecoder decoder(0xB0);
    FakeEncoder encoder;
    decoder.setNextResult(DomainEvent(TurnoutStateChanged(TurnoutAddress(5), TurnoutPosition::Closed)));
    LocoNetMessageRouter router(locoNetPort, mqttPort, clock, {{&decoder, &encoder}});
    locoNetPort.enqueue(LocoNetMessage({0xB0, 0x04, 0x30, 0x7B}));

    router.update();

    REQUIRE(mqttPort.published().size() == 1);
    REQUIRE(mqttPort.published()[0].topic() == "fake/topic");
}

TEST_CASE("a message no decoder can decode publishes nothing")
{
    FakeLocoNetPort locoNetPort;
    FakeMqttPort mqttPort;
    FakeClock clock;
    FakeDecoder decoder(0xB0);
    FakeEncoder encoder;
    LocoNetMessageRouter router(locoNetPort, mqttPort, clock, {{&decoder, &encoder}});
    locoNetPort.enqueue(LocoNetMessage({0x81}));

    router.update();

    REQUIRE(decoder.decodeCallCount() == 0);
    REQUIRE(mqttPort.published().size() == 0);
}

TEST_CASE("a decoder returning nullopt publishes nothing")
{
    FakeLocoNetPort locoNetPort;
    FakeMqttPort mqttPort;
    FakeClock clock;
    FakeDecoder decoder(0xB0);
    FakeEncoder encoder;
    decoder.setNextResult(std::nullopt);
    LocoNetMessageRouter router(locoNetPort, mqttPort, clock, {{&decoder, &encoder}});
    locoNetPort.enqueue(LocoNetMessage({0xB0, 0x04, 0x20, 0x6B}));

    router.update();

    REQUIRE(mqttPort.published().size() == 0);
}

TEST_CASE("republishes all known states once the republish interval elapses")
{
    FakeLocoNetPort locoNetPort;
    FakeMqttPort mqttPort;
    FakeClock clock;
    FakeDecoder decoder(0xB0);
    FakeEncoder encoder;
    decoder.setKnownStates({DomainEvent(TurnoutStateChanged(TurnoutAddress(5), TurnoutPosition::Closed))});
    LocoNetMessageRouter router(locoNetPort, mqttPort, clock, {{&decoder, &encoder}});
    clock.setNowMilliseconds(30000);

    router.update();

    REQUIRE(mqttPort.published().size() == 1);
}

TEST_CASE("does not republish before the republish interval elapses")
{
    FakeLocoNetPort locoNetPort;
    FakeMqttPort mqttPort;
    FakeClock clock;
    FakeDecoder decoder(0xB0);
    FakeEncoder encoder;
    decoder.setKnownStates({DomainEvent(TurnoutStateChanged(TurnoutAddress(5), TurnoutPosition::Closed))});
    LocoNetMessageRouter router(locoNetPort, mqttPort, clock, {{&decoder, &encoder}});
    clock.setNowMilliseconds(29999);

    router.update();

    REQUIRE(mqttPort.published().size() == 0);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `pio test -e native -f test_loco_net_message_router`
Expected: FAIL — `fatal error: application/LocoNetMessageRouter.h: No such file or directory`

- [ ] **Step 3: Write minimal implementation**

```cpp
// lib/Loco2MqttCore/src/ports/LocoNetMessageDecoder.h
#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "domain/DomainEvent.h"
#include "domain/LocoNetMessage.h"

class LocoNetMessageDecoder
{
public:
    virtual ~LocoNetMessageDecoder() = default;
    virtual bool canDecode(uint8_t opcode) const = 0;
    virtual std::optional<DomainEvent> decode(const LocoNetMessage& message) = 0;
    virtual std::vector<DomainEvent> allKnownStates() const = 0;
};
```

```cpp
// lib/Loco2MqttCore/src/ports/MqttEventEncoder.h
#pragma once

#include "domain/DomainEvent.h"
#include "domain/MqttMessage.h"

class MqttEventEncoder
{
public:
    virtual ~MqttEventEncoder() = default;
    virtual MqttMessage encode(const DomainEvent& event) const = 0;
};
```

```cpp
// lib/Loco2MqttCore/src/application/LocoNetMessageRouter.h
#pragma once

#include <utility>
#include <vector>

#include "ports/Clock.h"
#include "ports/LocoNetMessageDecoder.h"
#include "ports/LocoNetPort.h"
#include "ports/MqttEventEncoder.h"
#include "ports/MqttPort.h"

class LocoNetMessageRouter
{
public:
    LocoNetMessageRouter(LocoNetPort& locoNetPort, MqttPort& mqttPort, Clock& clock,
                          std::vector<std::pair<LocoNetMessageDecoder*, MqttEventEncoder*>> decoders)
        : locoNetPort_(locoNetPort), mqttPort_(mqttPort), clock_(clock), decoders_(std::move(decoders))
    {
    }

    void update();

private:
    void handleMessage(const LocoNetMessage& message);
    void republishAllKnownStatesIfDue();

    LocoNetPort& locoNetPort_;
    MqttPort& mqttPort_;
    Clock& clock_;
    std::vector<std::pair<LocoNetMessageDecoder*, MqttEventEncoder*>> decoders_;
    unsigned long lastRepublishAtMilliseconds_ = 0;

    static constexpr unsigned long kStateRepublishIntervalMs = 30000;
};
```

```cpp
// lib/Loco2MqttCore/src/application/LocoNetMessageRouter.cpp
#include "application/LocoNetMessageRouter.h"

void LocoNetMessageRouter::update()
{
    std::optional<LocoNetMessage> message = locoNetPort_.receive();
    while (message.has_value())
    {
        handleMessage(*message);
        message = locoNetPort_.receive();
    }
    republishAllKnownStatesIfDue();
}

void LocoNetMessageRouter::handleMessage(const LocoNetMessage& message)
{
    for (auto& [decoder, encoder] : decoders_)
    {
        if (!decoder->canDecode(message.bytes()[0]))
        {
            continue;
        }
        std::optional<DomainEvent> event = decoder->decode(message);
        if (event.has_value())
        {
            mqttPort_.publish(encoder->encode(*event));
        }
        return;
    }
}

void LocoNetMessageRouter::republishAllKnownStatesIfDue()
{
    if (clock_.nowMilliseconds() - lastRepublishAtMilliseconds_ < kStateRepublishIntervalMs)
    {
        return;
    }
    lastRepublishAtMilliseconds_ = clock_.nowMilliseconds();
    for (auto& [decoder, encoder] : decoders_)
    {
        for (const DomainEvent& event : decoder->allKnownStates())
        {
            mqttPort_.publish(encoder->encode(event));
        }
    }
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `pio test -e native -f test_loco_net_message_router`
Expected: PASS — 5 test cases, 7 assertions

- [ ] **Step 5: Commit**

```bash
git add lib/Loco2MqttCore/src/ports/LocoNetMessageDecoder.h lib/Loco2MqttCore/src/ports/MqttEventEncoder.h lib/Loco2MqttCore/src/application/LocoNetMessageRouter.h lib/Loco2MqttCore/src/application/LocoNetMessageRouter.cpp test/test_loco_net_message_router/test_main.cpp
git commit -m "feat: add LocoNetMessageDecoder/MqttEventEncoder ports and LocoNetMessageRouter"
```

---

### Task 8: MqttCommandDecoder, LocoNetEncoder ports, and MqttCommandRouter

**Files:**
- Create: `lib/Loco2MqttCore/src/ports/MqttCommandDecoder.h`
- Create: `lib/Loco2MqttCore/src/ports/LocoNetEncoder.h`
- Create: `lib/Loco2MqttCore/src/application/MqttCommandRouter.h`
- Create: `lib/Loco2MqttCore/src/application/MqttCommandRouter.cpp`
- Test: `test/test_mqtt_command_router/test_main.cpp`

**Interfaces:**
- Consumes: `DomainCommand` (Task 2), `IncomingMqttMessage` (Task 4),
  `MqttPort`/`FakeMqttPort` (Task 5), `LocoNetSendScheduler`/
  `FakeLocoNetSendScheduler` (Task 6).
- Produces: `MqttCommandDecoder` port — `bool canDecode(const
  std::string& deviceTypeSegment) const`, `std::optional<DomainCommand>
  decode(const std::string& address, const std::string& payload) const`.
  `LocoNetEncoder` port — `void encode(const DomainCommand&,
  LocoNetSendScheduler&) const`. `MqttCommandRouter(MqttPort&,
  LocoNetSendScheduler&, std::vector<std::pair<MqttCommandDecoder*,
  LocoNetEncoder*>> decoders)` with `void update()`.

**Test doubles for this task only** (same rationale as Task 7 — trivial,
test-file-local):

- [ ] **Step 1: Write the failing test**

```cpp
// test/test_mqtt_command_router/test_main.cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "application/MqttCommandRouter.h"
#include "domain/SetTurnoutPosition.h"
#include "domain/TurnoutAddress.h"
#include "domain/TurnoutPosition.h"
#include "support/FakeLocoNetSendScheduler.h"
#include "support/FakeMqttPort.h"

namespace
{
    class FakeDecoder : public MqttCommandDecoder
    {
    public:
        explicit FakeDecoder(std::string deviceType) : deviceType_(std::move(deviceType))
        {
        }

        bool canDecode(const std::string& deviceTypeSegment) const override
        {
            return deviceTypeSegment == deviceType_;
        }

        std::optional<DomainCommand> decode(const std::string& address, const std::string& payload) const override
        {
            lastAddress_ = address;
            lastPayload_ = payload;
            return nextResult_;
        }

        void setNextResult(std::optional<DomainCommand> result)
        {
            nextResult_ = result;
        }

        const std::string& lastAddress() const
        {
            return lastAddress_;
        }

        const std::string& lastPayload() const
        {
            return lastPayload_;
        }

    private:
        std::string deviceType_;
        mutable std::optional<DomainCommand> nextResult_;
        mutable std::string lastAddress_;
        mutable std::string lastPayload_;
    };

    class FakeEncoder : public LocoNetEncoder
    {
    public:
        void encode(const DomainCommand&, LocoNetSendScheduler& scheduler) const override
        {
            encodeCallCount_++;
            scheduler.sendNow(LocoNetMessage({0xB0, 0x00, 0x00, 0x4F}));
        }

        int encodeCallCount() const
        {
            return encodeCallCount_;
        }

    private:
        mutable int encodeCallCount_ = 0;
    };
}

TEST_CASE("dispatches a decoded command to the matching encoder")
{
    FakeMqttPort mqttPort;
    FakeLocoNetSendScheduler scheduler;
    FakeDecoder decoder("turnout");
    FakeEncoder encoder;
    decoder.setNextResult(DomainCommand(SetTurnoutPosition(TurnoutAddress(5), TurnoutPosition::Closed)));
    MqttCommandRouter router(mqttPort, scheduler, {{&decoder, &encoder}});
    mqttPort.enqueueCommand(IncomingMqttMessage("loconet/turnout/5/set", "CLOSED"));

    router.update();

    REQUIRE(encoder.encodeCallCount() == 1);
    REQUIRE(decoder.lastAddress() == "5");
    REQUIRE(decoder.lastPayload() == "CLOSED");
    REQUIRE(scheduler.sentNow().size() == 1);
}

TEST_CASE("a topic no decoder can decode dispatches nothing")
{
    FakeMqttPort mqttPort;
    FakeLocoNetSendScheduler scheduler;
    FakeDecoder decoder("turnout");
    FakeEncoder encoder;
    MqttCommandRouter router(mqttPort, scheduler, {{&decoder, &encoder}});
    mqttPort.enqueueCommand(IncomingMqttMessage("loconet/sensor/5/set", "CLOSED"));

    router.update();

    REQUIRE(encoder.encodeCallCount() == 0);
}

TEST_CASE("a decoder returning nullopt dispatches nothing")
{
    FakeMqttPort mqttPort;
    FakeLocoNetSendScheduler scheduler;
    FakeDecoder decoder("turnout");
    FakeEncoder encoder;
    decoder.setNextResult(std::nullopt);
    MqttCommandRouter router(mqttPort, scheduler, {{&decoder, &encoder}});
    mqttPort.enqueueCommand(IncomingMqttMessage("loconet/turnout/5/set", "SIDEWAYS"));

    router.update();

    REQUIRE(encoder.encodeCallCount() == 0);
}

TEST_CASE("a malformed topic with too few segments dispatches nothing")
{
    FakeMqttPort mqttPort;
    FakeLocoNetSendScheduler scheduler;
    FakeDecoder decoder("turnout");
    FakeEncoder encoder;
    MqttCommandRouter router(mqttPort, scheduler, {{&decoder, &encoder}});
    mqttPort.enqueueCommand(IncomingMqttMessage("loconet/turnout", "CLOSED"));

    router.update();

    REQUIRE(encoder.encodeCallCount() == 0);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `pio test -e native -f test_mqtt_command_router`
Expected: FAIL — `fatal error: application/MqttCommandRouter.h: No such file or directory`

- [ ] **Step 3: Write minimal implementation**

```cpp
// lib/Loco2MqttCore/src/ports/MqttCommandDecoder.h
#pragma once

#include <optional>
#include <string>

#include "domain/DomainCommand.h"

class MqttCommandDecoder
{
public:
    virtual ~MqttCommandDecoder() = default;
    virtual bool canDecode(const std::string& deviceTypeSegment) const = 0;
    virtual std::optional<DomainCommand> decode(const std::string& address, const std::string& payload) const = 0;
};
```

```cpp
// lib/Loco2MqttCore/src/ports/LocoNetEncoder.h
#pragma once

#include "domain/DomainCommand.h"
#include "ports/LocoNetSendScheduler.h"

class LocoNetEncoder
{
public:
    virtual ~LocoNetEncoder() = default;
    virtual void encode(const DomainCommand& command, LocoNetSendScheduler& scheduler) const = 0;
};
```

```cpp
// lib/Loco2MqttCore/src/application/MqttCommandRouter.h
#pragma once

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ports/LocoNetEncoder.h"
#include "ports/MqttCommandDecoder.h"
#include "ports/MqttPort.h"

class MqttCommandRouter
{
public:
    MqttCommandRouter(MqttPort& mqttPort, LocoNetSendScheduler& scheduler,
                       std::vector<std::pair<MqttCommandDecoder*, LocoNetEncoder*>> decoders)
        : mqttPort_(mqttPort), scheduler_(scheduler), decoders_(std::move(decoders))
    {
    }

    void update();

private:
    void handleMessage(const IncomingMqttMessage& message);

    MqttPort& mqttPort_;
    LocoNetSendScheduler& scheduler_;
    std::vector<std::pair<MqttCommandDecoder*, LocoNetEncoder*>> decoders_;
};
```

```cpp
// lib/Loco2MqttCore/src/application/MqttCommandRouter.cpp
#include "application/MqttCommandRouter.h"

namespace
{
    std::optional<std::pair<std::string, std::string>> parseDeviceTypeAndAddress(const std::string& topic)
    {
        const auto firstSlash = topic.find('/');
        const auto secondSlash = firstSlash == std::string::npos ? std::string::npos : topic.find('/', firstSlash + 1);
        const auto thirdSlash = secondSlash == std::string::npos ? std::string::npos : topic.find('/', secondSlash + 1);
        if (firstSlash == std::string::npos || secondSlash == std::string::npos || thirdSlash == std::string::npos)
        {
            return std::nullopt;
        }
        return std::make_pair(topic.substr(firstSlash + 1, secondSlash - firstSlash - 1),
                               topic.substr(secondSlash + 1, thirdSlash - secondSlash - 1));
    }
}

void MqttCommandRouter::update()
{
    std::optional<IncomingMqttMessage> message = mqttPort_.receiveCommand();
    while (message.has_value())
    {
        handleMessage(*message);
        message = mqttPort_.receiveCommand();
    }
}

void MqttCommandRouter::handleMessage(const IncomingMqttMessage& message)
{
    const auto parsed = parseDeviceTypeAndAddress(message.topic());
    if (!parsed.has_value())
    {
        return;
    }
    for (auto& [decoder, encoder] : decoders_)
    {
        if (!decoder->canDecode(parsed->first))
        {
            continue;
        }
        std::optional<DomainCommand> command = decoder->decode(parsed->second, message.payload());
        if (command.has_value())
        {
            encoder->encode(*command, scheduler_);
        }
        return;
    }
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `pio test -e native -f test_mqtt_command_router`
Expected: PASS — 4 test cases, 8 assertions

- [ ] **Step 5: Commit**

```bash
git add lib/Loco2MqttCore/src/ports/MqttCommandDecoder.h lib/Loco2MqttCore/src/ports/LocoNetEncoder.h lib/Loco2MqttCore/src/application/MqttCommandRouter.h lib/Loco2MqttCore/src/application/MqttCommandRouter.cpp test/test_mqtt_command_router/test_main.cpp
git commit -m "feat: add MqttCommandDecoder/LocoNetEncoder ports and MqttCommandRouter"
```

---

### Task 9: TurnoutLocoNetDecoder

**Files:**
- Create: `lib/Loco2MqttCore/src/turnout/TurnoutLocoNetDecoder.h`
- Create: `lib/Loco2MqttCore/src/turnout/TurnoutLocoNetDecoder.cpp`
- Test: `test/test_turnout_loco_net_decoder/test_main.cpp`

**Interfaces:**
- Consumes: `LocoNetMessageDecoder` port (Task 7), `TurnoutAddress`
  (Task 1), `TurnoutPosition`/`TurnoutStateChanged`/`DomainEvent`
  (Task 2).
- Produces: `TurnoutLocoNetDecoder implements LocoNetMessageDecoder`.

**Byte layout for this task** (copy verbatim from the Global Constraints
citation — do not re-derive):
- `canDecode(opcode)`: `true` for `0xB0` (`OPC_SW_REQ`) or `0xB1`
  (`OPC_SW_REP`).
- For `0xB0`: `address = (((message.bytes()[2] & 0x0F) << 7) |
  (message.bytes()[1] & 0x7F)) + 1`; `position = (message.bytes()[2] &
  0x20) ? Closed : Thrown` (the `0x10` on/off bit is ignored — both the
  on-pulse and its later off-pulse describe the same commanded position).
- For `0xB1`: if `message.bytes()[2] & 0x40` is set, this is a
  sensor/input report — return `std::nullopt` regardless of decoder
  state. Otherwise, same address/position formula as `0xB0`.
- Dedup: keep `std::map<int, TurnoutPosition> lastKnownPosition_` keyed
  by the address's `.value()`. If the address isn't in the map yet, or
  its stored position differs from the newly decoded one, update the map
  and return the event. Otherwise return `std::nullopt`.
- `allKnownStates()`: return one `TurnoutStateChanged` per entry in
  `lastKnownPosition_`.

- [ ] **Step 1: Write the failing test**

```cpp
// test/test_turnout_loco_net_decoder/test_main.cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/TurnoutStateChanged.h"
#include "turnout/TurnoutLocoNetDecoder.h"

TEST_CASE("canDecode is true for OPC_SW_REQ and OPC_SW_REP, false otherwise")
{
    TurnoutLocoNetDecoder decoder;

    REQUIRE(decoder.canDecode(0xB0) == true);
    REQUIRE(decoder.canDecode(0xB1) == true);
    REQUIRE(decoder.canDecode(0x81) == false);
}

TEST_CASE("decodes an OPC_SW_REQ on-pulse for address 5, Closed")
{
    TurnoutLocoNetDecoder decoder;

    auto event = decoder.decode(LocoNetMessage({0xB0, 0x04, 0x30, 0x7B}));

    REQUIRE(event.has_value());
    auto& state = std::get<TurnoutStateChanged>(*event);
    REQUIRE(state.address().value() == 5);
    REQUIRE(state.position() == TurnoutPosition::Closed);
}

TEST_CASE("decodes an OPC_SW_REQ for address 5, Thrown")
{
    TurnoutLocoNetDecoder decoder;

    auto event = decoder.decode(LocoNetMessage({0xB0, 0x04, 0x10, 0x5B}));

    REQUIRE(event.has_value());
    REQUIRE(std::get<TurnoutStateChanged>(*event).position() == TurnoutPosition::Thrown);
}

TEST_CASE("decodes the maximum address, 2048")
{
    TurnoutLocoNetDecoder decoder;

    auto event = decoder.decode(LocoNetMessage({0xB0, 0x7F, 0x3F, 0x0F}));

    REQUIRE(event.has_value());
    REQUIRE(std::get<TurnoutStateChanged>(*event).address().value() == 2048);
}

TEST_CASE("the off-pulse following an on-pulse is deduped as no change")
{
    TurnoutLocoNetDecoder decoder;
    decoder.decode(LocoNetMessage({0xB0, 0x04, 0x30, 0x7B}));

    auto event = decoder.decode(LocoNetMessage({0xB0, 0x04, 0x20, 0x6B}));

    REQUIRE(event.has_value() == false);
}

TEST_CASE("a genuine position change after a known state is not deduped")
{
    TurnoutLocoNetDecoder decoder;
    decoder.decode(LocoNetMessage({0xB0, 0x04, 0x30, 0x7B}));

    auto event = decoder.decode(LocoNetMessage({0xB0, 0x04, 0x10, 0x5B}));

    REQUIRE(event.has_value());
    REQUIRE(std::get<TurnoutStateChanged>(*event).position() == TurnoutPosition::Thrown);
}

TEST_CASE("decodes an OPC_SW_REP switch report as a new state")
{
    TurnoutLocoNetDecoder decoder;

    auto event = decoder.decode(LocoNetMessage({0xB1, 0x04, 0x20, 0x6A}));

    REQUIRE(event.has_value());
    auto& state = std::get<TurnoutStateChanged>(*event);
    REQUIRE(state.address().value() == 5);
    REQUIRE(state.position() == TurnoutPosition::Closed);
}

TEST_CASE("an OPC_SW_REP sensor report is not a commanded-position event")
{
    TurnoutLocoNetDecoder decoder;

    auto event = decoder.decode(LocoNetMessage({0xB1, 0x04, 0x40, 0x0A}));

    REQUIRE(event.has_value() == false);
}

TEST_CASE("allKnownStates returns every address seen so far")
{
    TurnoutLocoNetDecoder decoder;
    decoder.decode(LocoNetMessage({0xB0, 0x04, 0x30, 0x7B}));
    decoder.decode(LocoNetMessage({0xB0, 0x06, 0x10, 0x59}));

    auto states = decoder.allKnownStates();

    REQUIRE(states.size() == 2);
    REQUIRE(std::get<TurnoutStateChanged>(states[0]).address().value() == 5);
    REQUIRE(std::get<TurnoutStateChanged>(states[1]).address().value() == 7);
}

TEST_CASE("allKnownStates is empty before anything has been decoded")
{
    TurnoutLocoNetDecoder decoder;

    REQUIRE(decoder.allKnownStates().empty());
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `pio test -e native -f test_turnout_loco_net_decoder`
Expected: FAIL — `fatal error: turnout/TurnoutLocoNetDecoder.h: No such file or directory`

- [ ] **Step 3: Write minimal implementation**

```cpp
// lib/Loco2MqttCore/src/turnout/TurnoutLocoNetDecoder.h
#pragma once

#include <map>

#include "ports/LocoNetMessageDecoder.h"

class TurnoutLocoNetDecoder : public LocoNetMessageDecoder
{
public:
    bool canDecode(uint8_t opcode) const override;
    std::optional<DomainEvent> decode(const LocoNetMessage& message) override;
    std::vector<DomainEvent> allKnownStates() const override;

private:
    std::map<int, TurnoutPosition> lastKnownPosition_;
};
```

Note: `TurnoutPosition` is reached transitively via `domain/DomainEvent.h`
→ `domain/TurnoutStateChanged.h` → `domain/TurnoutPosition.h`, all
already included through `ports/LocoNetMessageDecoder.h`. Add an explicit
`#include "domain/TurnoutPosition.h"` and `#include
"domain/TurnoutAddress.h"` to this header for clarity and to satisfy the
native-link rule for the classes this header names directly.

```cpp
// lib/Loco2MqttCore/src/turnout/TurnoutLocoNetDecoder.h (final version)
#pragma once

#include <map>

#include "domain/TurnoutAddress.h"
#include "domain/TurnoutPosition.h"
#include "domain/TurnoutStateChanged.h"
#include "ports/LocoNetMessageDecoder.h"

class TurnoutLocoNetDecoder : public LocoNetMessageDecoder
{
public:
    bool canDecode(uint8_t opcode) const override;
    std::optional<DomainEvent> decode(const LocoNetMessage& message) override;
    std::vector<DomainEvent> allKnownStates() const override;

private:
    std::map<int, TurnoutPosition> lastKnownPosition_;
};
```

```cpp
// lib/Loco2MqttCore/src/turnout/TurnoutLocoNetDecoder.cpp
#include "turnout/TurnoutLocoNetDecoder.h"

namespace
{
    constexpr uint8_t kOpcSwReq = 0xB0;
    constexpr uint8_t kOpcSwRep = 0xB1;
    constexpr uint8_t kSwRepInputs = 0x40;
    constexpr uint8_t kClosedBit = 0x20;

    int addressFrom(const LocoNetMessage& message)
    {
        return (((message.bytes()[2] & 0x0F) << 7) | (message.bytes()[1] & 0x7F)) + 1;
    }

    TurnoutPosition positionFrom(const LocoNetMessage& message)
    {
        return (message.bytes()[2] & kClosedBit) ? TurnoutPosition::Closed : TurnoutPosition::Thrown;
    }
}

bool TurnoutLocoNetDecoder::canDecode(uint8_t opcode) const
{
    return opcode == kOpcSwReq || opcode == kOpcSwRep;
}

std::optional<DomainEvent> TurnoutLocoNetDecoder::decode(const LocoNetMessage& message)
{
    if (message.bytes()[0] == kOpcSwRep && (message.bytes()[2] & kSwRepInputs))
    {
        return std::nullopt;
    }
    const int address = addressFrom(message);
    const TurnoutPosition position = positionFrom(message);
    auto existing = lastKnownPosition_.find(address);
    if (existing != lastKnownPosition_.end() && existing->second == position)
    {
        return std::nullopt;
    }
    lastKnownPosition_[address] = position;
    return DomainEvent(TurnoutStateChanged(TurnoutAddress(address), position));
}

std::vector<DomainEvent> TurnoutLocoNetDecoder::allKnownStates() const
{
    std::vector<DomainEvent> states;
    for (const auto& [address, position] : lastKnownPosition_)
    {
        states.emplace_back(TurnoutStateChanged(TurnoutAddress(address), position));
    }
    return states;
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `pio test -e native -f test_turnout_loco_net_decoder`
Expected: PASS — 10 test cases, 18 assertions

- [ ] **Step 5: Commit**

```bash
git add lib/Loco2MqttCore/src/turnout/TurnoutLocoNetDecoder.h lib/Loco2MqttCore/src/turnout/TurnoutLocoNetDecoder.cpp test/test_turnout_loco_net_decoder/test_main.cpp
git commit -m "feat: add TurnoutLocoNetDecoder"
```

---

### Task 10: TurnoutMqttEncoder

**Files:**
- Create: `lib/Loco2MqttCore/src/turnout/TurnoutMqttEncoder.h`
- Create: `lib/Loco2MqttCore/src/turnout/TurnoutMqttEncoder.cpp`
- Test: `test/test_turnout_mqtt_encoder/test_main.cpp`

**Interfaces:**
- Consumes: `MqttEventEncoder` port (Task 7), `DomainEvent`/
  `TurnoutStateChanged` (Task 2), `MqttMessage` (Task 4).
- Produces: `TurnoutMqttEncoder implements MqttEventEncoder`.

- [ ] **Step 1: Write the failing test**

```cpp
// test/test_turnout_mqtt_encoder/test_main.cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/TurnoutAddress.h"
#include "domain/TurnoutStateChanged.h"
#include "turnout/TurnoutMqttEncoder.h"

TEST_CASE("encodes a Closed event to the state topic")
{
    TurnoutMqttEncoder encoder;

    MqttMessage message = encoder.encode(DomainEvent(TurnoutStateChanged(TurnoutAddress(5), TurnoutPosition::Closed)));

    REQUIRE(message.topic() == "loconet/turnout/5/state");
    REQUIRE(message.payload() == "CLOSED");
    REQUIRE(message.retained() == true);
}

TEST_CASE("encodes a Thrown event with the correct payload")
{
    TurnoutMqttEncoder encoder;

    MqttMessage message = encoder.encode(DomainEvent(TurnoutStateChanged(TurnoutAddress(7), TurnoutPosition::Thrown)));

    REQUIRE(message.topic() == "loconet/turnout/7/state");
    REQUIRE(message.payload() == "THROWN");
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `pio test -e native -f test_turnout_mqtt_encoder`
Expected: FAIL — `fatal error: turnout/TurnoutMqttEncoder.h: No such file or directory`

- [ ] **Step 3: Write minimal implementation**

```cpp
// lib/Loco2MqttCore/src/turnout/TurnoutMqttEncoder.h
#pragma once

#include "domain/TurnoutStateChanged.h"
#include "ports/MqttEventEncoder.h"

class TurnoutMqttEncoder : public MqttEventEncoder
{
public:
    MqttMessage encode(const DomainEvent& event) const override;
};
```

```cpp
// lib/Loco2MqttCore/src/turnout/TurnoutMqttEncoder.cpp
#include "turnout/TurnoutMqttEncoder.h"

MqttMessage TurnoutMqttEncoder::encode(const DomainEvent& event) const
{
    const auto& state = std::get<TurnoutStateChanged>(event);
    const std::string topic = "loconet/turnout/" + std::to_string(state.address().value()) + "/state";
    const std::string payload = state.position() == TurnoutPosition::Closed ? "CLOSED" : "THROWN";
    return MqttMessage(topic, payload, true);
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `pio test -e native -f test_turnout_mqtt_encoder`
Expected: PASS — 2 test cases, 5 assertions

- [ ] **Step 5: Commit**

```bash
git add lib/Loco2MqttCore/src/turnout/TurnoutMqttEncoder.h lib/Loco2MqttCore/src/turnout/TurnoutMqttEncoder.cpp test/test_turnout_mqtt_encoder/test_main.cpp
git commit -m "feat: add TurnoutMqttEncoder"
```

---

### Task 11: TurnoutMqttCommandDecoder

**Files:**
- Create: `lib/Loco2MqttCore/src/turnout/TurnoutMqttCommandDecoder.h`
- Create: `lib/Loco2MqttCore/src/turnout/TurnoutMqttCommandDecoder.cpp`
- Test: `test/test_turnout_mqtt_command_decoder/test_main.cpp`

**Interfaces:**
- Consumes: `MqttCommandDecoder` port (Task 8), `DomainCommand`/
  `SetTurnoutPosition`/`TurnoutAddress` (Tasks 1-2).
- Produces: `TurnoutMqttCommandDecoder implements MqttCommandDecoder`.

- [ ] **Step 1: Write the failing test**

```cpp
// test/test_turnout_mqtt_command_decoder/test_main.cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/SetTurnoutPosition.h"
#include "turnout/TurnoutMqttCommandDecoder.h"

TEST_CASE("canDecode is true only for the turnout device type")
{
    TurnoutMqttCommandDecoder decoder;

    REQUIRE(decoder.canDecode("turnout") == true);
    REQUIRE(decoder.canDecode("sensor") == false);
}

TEST_CASE("decodes a valid CLOSED command")
{
    TurnoutMqttCommandDecoder decoder;

    auto command = decoder.decode("5", "CLOSED");

    REQUIRE(command.has_value());
    auto& set = std::get<SetTurnoutPosition>(*command);
    REQUIRE(set.address().value() == 5);
    REQUIRE(set.position() == TurnoutPosition::Closed);
}

TEST_CASE("decodes a valid THROWN command")
{
    TurnoutMqttCommandDecoder decoder;

    auto command = decoder.decode("2048", "THROWN");

    REQUIRE(command.has_value());
    auto& set = std::get<SetTurnoutPosition>(*command);
    REQUIRE(set.address().value() == 2048);
    REQUIRE(set.position() == TurnoutPosition::Thrown);
}

TEST_CASE("rejects a non-numeric address")
{
    TurnoutMqttCommandDecoder decoder;

    REQUIRE(decoder.decode("abc", "CLOSED").has_value() == false);
}

TEST_CASE("rejects an address below the valid range")
{
    TurnoutMqttCommandDecoder decoder;

    REQUIRE(decoder.decode("0", "CLOSED").has_value() == false);
}

TEST_CASE("rejects an address above the valid range")
{
    TurnoutMqttCommandDecoder decoder;

    REQUIRE(decoder.decode("2049", "CLOSED").has_value() == false);
}

TEST_CASE("rejects a payload that is not exactly CLOSED or THROWN")
{
    TurnoutMqttCommandDecoder decoder;

    REQUIRE(decoder.decode("5", "closed").has_value() == false);
    REQUIRE(decoder.decode("5", "SIDEWAYS").has_value() == false);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `pio test -e native -f test_turnout_mqtt_command_decoder`
Expected: FAIL — `fatal error: turnout/TurnoutMqttCommandDecoder.h: No such file or directory`

- [ ] **Step 3: Write minimal implementation**

```cpp
// lib/Loco2MqttCore/src/turnout/TurnoutMqttCommandDecoder.h
#pragma once

#include "domain/SetTurnoutPosition.h"
#include "ports/MqttCommandDecoder.h"

class TurnoutMqttCommandDecoder : public MqttCommandDecoder
{
public:
    bool canDecode(const std::string& deviceTypeSegment) const override;
    std::optional<DomainCommand> decode(const std::string& address, const std::string& payload) const override;
};
```

```cpp
// lib/Loco2MqttCore/src/turnout/TurnoutMqttCommandDecoder.cpp
#include "turnout/TurnoutMqttCommandDecoder.h"

namespace
{
    bool isValidAddressString(const std::string& address)
    {
        if (address.empty() || address.size() > 4)
        {
            return false;
        }
        for (char c : address)
        {
            if (c < '0' || c > '9')
            {
                return false;
            }
        }
        return true;
    }
}

bool TurnoutMqttCommandDecoder::canDecode(const std::string& deviceTypeSegment) const
{
    return deviceTypeSegment == "turnout";
}

std::optional<DomainCommand> TurnoutMqttCommandDecoder::decode(const std::string& address,
                                                                 const std::string& payload) const
{
    if (!isValidAddressString(address))
    {
        return std::nullopt;
    }
    const int value = std::stoi(address);
    if (value < 1 || value > 2048)
    {
        return std::nullopt;
    }
    if (payload != "CLOSED" && payload != "THROWN")
    {
        return std::nullopt;
    }
    const TurnoutPosition position = payload == "CLOSED" ? TurnoutPosition::Closed : TurnoutPosition::Thrown;
    return DomainCommand(SetTurnoutPosition(TurnoutAddress(value), position));
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `pio test -e native -f test_turnout_mqtt_command_decoder`
Expected: PASS — 7 test cases, 12 assertions

- [ ] **Step 5: Commit**

```bash
git add lib/Loco2MqttCore/src/turnout/TurnoutMqttCommandDecoder.h lib/Loco2MqttCore/src/turnout/TurnoutMqttCommandDecoder.cpp test/test_turnout_mqtt_command_decoder/test_main.cpp
git commit -m "feat: add TurnoutMqttCommandDecoder"
```

---

### Task 12: TurnoutLocoNetEncoder

**Files:**
- Create: `lib/Loco2MqttCore/src/turnout/TurnoutLocoNetEncoder.h`
- Create: `lib/Loco2MqttCore/src/turnout/TurnoutLocoNetEncoder.cpp`
- Test: `test/test_turnout_loco_net_encoder/test_main.cpp`

**Interfaces:**
- Consumes: `LocoNetEncoder` port (Task 8), `LocoNetSendScheduler`/
  `FakeLocoNetSendScheduler` (Task 6), `DomainCommand`/
  `SetTurnoutPosition` (Task 2), `computeLocoNetChecksum` (Task 3).
- Produces: `TurnoutLocoNetEncoder implements LocoNetEncoder`.

**Byte layout for this task** (copy verbatim — see Global Constraints):
`SW1 = (address - 1) & 0x7F`; `SW2 = (((address - 1) >> 7) & 0x0F) |
(position == Closed ? 0x20 : 0x00) | (outputOn ? 0x10 : 0x00)`;
`CHECKSUM = computeLocoNetChecksum({0xB0, SW1, SW2})`. The on-pulse has
`outputOn = true`, sent via `scheduler.sendNow(...)`; the off-pulse has
`outputOn = false`, same `SW1`, sent via `scheduler.sendAfter(...,
kOffPulseDelayMs)` with `kOffPulseDelayMs = 250`.

- [ ] **Step 1: Write the failing test**

```cpp
// test/test_turnout_loco_net_encoder/test_main.cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/SetTurnoutPosition.h"
#include "domain/TurnoutAddress.h"
#include "support/FakeLocoNetSendScheduler.h"
#include "turnout/TurnoutLocoNetEncoder.h"

TEST_CASE("sends the on-pulse immediately for a Closed command")
{
    TurnoutLocoNetEncoder encoder;
    FakeLocoNetSendScheduler scheduler;

    encoder.encode(DomainCommand(SetTurnoutPosition(TurnoutAddress(5), TurnoutPosition::Closed)), scheduler);

    REQUIRE(scheduler.sentNow().size() == 1);
    REQUIRE(scheduler.sentNow()[0].bytes() == std::vector<uint8_t>{0xB0, 0x04, 0x30, 0x7B});
}

TEST_CASE("schedules the off-pulse 250ms later, same address and direction, output off")
{
    TurnoutLocoNetEncoder encoder;
    FakeLocoNetSendScheduler scheduler;

    encoder.encode(DomainCommand(SetTurnoutPosition(TurnoutAddress(5), TurnoutPosition::Closed)), scheduler);

    REQUIRE(scheduler.scheduled().size() == 1);
    REQUIRE(scheduler.scheduled()[0].first.bytes() == std::vector<uint8_t>{0xB0, 0x04, 0x20, 0x6B});
    REQUIRE(scheduler.scheduled()[0].second == 250);
}

TEST_CASE("encodes a Thrown command with the direction bit clear")
{
    TurnoutLocoNetEncoder encoder;
    FakeLocoNetSendScheduler scheduler;

    encoder.encode(DomainCommand(SetTurnoutPosition(TurnoutAddress(5), TurnoutPosition::Thrown)), scheduler);

    REQUIRE(scheduler.sentNow()[0].bytes() == std::vector<uint8_t>{0xB0, 0x04, 0x10, 0x5B});
    REQUIRE(scheduler.scheduled()[0].first.bytes() == std::vector<uint8_t>{0xB0, 0x04, 0x00, 0x4F});
}

TEST_CASE("encodes the maximum address, 2048")
{
    TurnoutLocoNetEncoder encoder;
    FakeLocoNetSendScheduler scheduler;

    encoder.encode(DomainCommand(SetTurnoutPosition(TurnoutAddress(2048), TurnoutPosition::Closed)), scheduler);

    REQUIRE(scheduler.sentNow()[0].bytes() == std::vector<uint8_t>{0xB0, 0x7F, 0x3F, 0x0F});
}
```

`0x4F` above is the off-pulse checksum for address 5, Thrown (`SW2 =
0x00`): `0xB0 ^ 0x04 ^ 0x00 ^ 0x4F == 0xFF` — hand-verified the same way
as the on-pulse values in Task 3.

- [ ] **Step 2: Run test to verify it fails**

Run: `pio test -e native -f test_turnout_loco_net_encoder`
Expected: FAIL — `fatal error: turnout/TurnoutLocoNetEncoder.h: No such file or directory`

- [ ] **Step 3: Write minimal implementation**

```cpp
// lib/Loco2MqttCore/src/turnout/TurnoutLocoNetEncoder.h
#pragma once

#include "ports/LocoNetEncoder.h"

class TurnoutLocoNetEncoder : public LocoNetEncoder
{
public:
    void encode(const DomainCommand& command, LocoNetSendScheduler& scheduler) const override;

private:
    static constexpr unsigned long kOffPulseDelayMs = 250;
};
```

```cpp
// lib/Loco2MqttCore/src/turnout/TurnoutLocoNetEncoder.cpp
#include "turnout/TurnoutLocoNetEncoder.h"

#include "domain/LocoNetChecksum.h"
#include "domain/SetTurnoutPosition.h"

namespace
{
    constexpr uint8_t kOpcSwReq = 0xB0;
    constexpr uint8_t kClosedBit = 0x20;
    constexpr uint8_t kOutputOnBit = 0x10;

    LocoNetMessage buildSwReq(const SetTurnoutPosition& command, bool outputOn)
    {
        const int zeroBasedAddress = command.address().value() - 1;
        const uint8_t sw1 = zeroBasedAddress & 0x7F;
        uint8_t sw2 = (zeroBasedAddress >> 7) & 0x0F;
        sw2 |= command.position() == TurnoutPosition::Closed ? kClosedBit : 0x00;
        sw2 |= outputOn ? kOutputOnBit : 0x00;
        const uint8_t checksum = computeLocoNetChecksum({kOpcSwReq, sw1, sw2});
        return LocoNetMessage({kOpcSwReq, sw1, sw2, checksum});
    }
}

void TurnoutLocoNetEncoder::encode(const DomainCommand& command, LocoNetSendScheduler& scheduler) const
{
    const auto& setPosition = std::get<SetTurnoutPosition>(command);
    scheduler.sendNow(buildSwReq(setPosition, /* outputOn = */ true));
    scheduler.sendAfter(buildSwReq(setPosition, /* outputOn = */ false), kOffPulseDelayMs);
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `pio test -e native -f test_turnout_loco_net_encoder`
Expected: PASS — 4 test cases, 8 assertions

- [ ] **Step 5: Commit**

```bash
git add lib/Loco2MqttCore/src/turnout/TurnoutLocoNetEncoder.h lib/Loco2MqttCore/src/turnout/TurnoutLocoNetEncoder.cpp test/test_turnout_loco_net_encoder/test_main.cpp
git commit -m "feat: add TurnoutLocoNetEncoder"
```

---

### Task 13: EspWifiPort (build-check only)

**Files:**
- Create: `lib/Loco2MqttCore/src/adapters/EspWifiPort.h`
- Create: `lib/Loco2MqttCore/src/adapters/EspWifiPort.cpp`

**Interfaces:**
- Consumes: `LocoNetAdapterConfig` (existing, for `wifiSsid()`/
  `wifiPassword()`).
- Produces: `EspWifiPort(const std::string& ssid, const std::string&
  password)` with `void update()` — no port interface, since nothing else
  in this codebase needs to be decoupled from WiFi connection status; it
  is constructed and driven directly from `main.cpp`, matching how
  `LocoNetEsp32Port` is used today.

This is a genuine hardware shim — no native test, matching
`LocoNetEsp32Port`'s existing precedent (build-check only via `pio run -e
esp32dev`).

- [ ] **Step 1: Write the implementation**

```cpp
// lib/Loco2MqttCore/src/adapters/EspWifiPort.h
#pragma once

#ifdef ARDUINO

#include <WiFi.h>

#include <string>

class EspWifiPort
{
public:
    EspWifiPort(std::string ssid, std::string password);

    void update();

private:
    std::string ssid_;
    std::string password_;
    unsigned long lastAttemptAtMillis_ = 0;

    static constexpr unsigned long kRetryIntervalMs = 5000;
};

#endif
```

```cpp
// lib/Loco2MqttCore/src/adapters/EspWifiPort.cpp
#ifdef ARDUINO

#include "adapters/EspWifiPort.h"

EspWifiPort::EspWifiPort(std::string ssid, std::string password)
    : ssid_(std::move(ssid)), password_(std::move(password))
{
}

void EspWifiPort::update()
{
    if (WiFi.status() == WL_CONNECTED)
    {
        return;
    }
    const unsigned long now = millis();
    if (now - lastAttemptAtMillis_ < kRetryIntervalMs)
    {
        return;
    }
    lastAttemptAtMillis_ = now;
    WiFi.begin(ssid_.c_str(), password_.c_str());
}

#endif
```

- [ ] **Step 2: Build-check against the real target**

Run: `pio run -e esp32dev`
Expected: SUCCESS (this file isn't referenced by `main.cpp` yet, so the
linker drops its object code — expected until Task 15 wires it in; the
build succeeding confirms it compiles against the real ESP32/Arduino
toolchain)

- [ ] **Step 3: Confirm no native regressions**

Run: `pio test -e native`
Expected: PASS — unchanged suite count from Task 12 (this file is
`#ifdef ARDUINO`-guarded and invisible to the native build)

- [ ] **Step 4: Commit**

```bash
git add lib/Loco2MqttCore/src/adapters/EspWifiPort.h lib/Loco2MqttCore/src/adapters/EspWifiPort.cpp
git commit -m "feat: add EspWifiPort adapter, build-checked against esp32dev"
```

---

### Task 14: PicoMqttPort (build-check only) and the PicoMQTT dependency

**Files:**
- Modify: `platformio.ini` (add PicoMQTT to `[env:esp32dev]` `lib_deps`)
- Create: `lib/Loco2MqttCore/src/adapters/PicoMqttPort.h`
- Create: `lib/Loco2MqttCore/src/adapters/PicoMqttPort.cpp`

**Interfaces:**
- Consumes: `MqttPort` port (Task 5).
- Produces: `PicoMqttPort implements MqttPort`, plus `void begin()` and
  `void update()` (not part of the port — concrete-only lifecycle methods,
  matching `LocoNetEsp32Port`'s `begin()`/`update()` shape).

Genuine hardware shim — no native test, build-check only.

- [ ] **Step 1: Add the pinned dependency**

Edit `platformio.ini`'s `[env:esp32dev]` section:

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200
lib_ldf_mode = deep+
lib_deps =
    https://github.com/tanner87661/LocoNetESP32HB.git#959207d3f82c49356bcf370940d590cad47afb19
    https://github.com/bblanchon/ArduinoJson.git#f9fe8557f13d8949eafd49ebd93a2929c4e5065f
    https://github.com/mlesniew/PicoMQTT.git#8518da5fd5fa3d1c147150f76957a0e3ecf40ab4
build_unflags = -std=gnu++11
build_flags = -std=gnu++17
```

- [ ] **Step 2: Write the implementation**

```cpp
// lib/Loco2MqttCore/src/adapters/PicoMqttPort.h
#pragma once

#ifdef ARDUINO

#include <PicoMQTT.h>

#include "ports/MqttPort.h"

class PicoMqttPort : public MqttPort
{
public:
    PicoMqttPort();

    void begin();
    void update();
    void publish(const MqttMessage& message) override;
    std::optional<IncomingMqttMessage> receiveCommand() override;

private:
    PicoMQTT::Server server_;
    std::deque<IncomingMqttMessage> pendingCommands_;
};

#endif
```

- [ ] **Step 3: Run test to verify the header alone doesn't yet build (no .cpp)**

This step is intentionally skipped for build-check-only adapters — see
Task 13's note. Proceed directly to the implementation.

- [ ] **Step 4: Write the .cpp and confirm the build**

```cpp
// lib/Loco2MqttCore/src/adapters/PicoMqttPort.cpp
#ifdef ARDUINO

#include "adapters/PicoMqttPort.h"

PicoMqttPort::PicoMqttPort()
{
    server_.subscribe("loconet/+/+/set", [this](const char* topic, const char* payload)
    {
        pendingCommands_.emplace_back(std::string(topic), std::string(payload));
    });
}

void PicoMqttPort::begin()
{
    server_.begin();
}

void PicoMqttPort::update()
{
    server_.loop();
}

void PicoMqttPort::publish(const MqttMessage& message)
{
    server_.publish(message.topic().c_str(), message.payload().c_str());
}

std::optional<IncomingMqttMessage> PicoMqttPort::receiveCommand()
{
    if (pendingCommands_.empty())
    {
        return std::nullopt;
    }
    IncomingMqttMessage message = pendingCommands_.front();
    pendingCommands_.pop_front();
    return message;
}

#endif
```

Add `#include <deque>` and `#include <string>` to `PicoMqttPort.h` for
`pendingCommands_`'s type — final header:

```cpp
// lib/Loco2MqttCore/src/adapters/PicoMqttPort.h (final version)
#pragma once

#ifdef ARDUINO

#include <PicoMQTT.h>

#include <deque>
#include <string>

#include "ports/MqttPort.h"

class PicoMqttPort : public MqttPort
{
public:
    PicoMqttPort();

    void begin();
    void update();
    void publish(const MqttMessage& message) override;
    std::optional<IncomingMqttMessage> receiveCommand() override;

private:
    PicoMQTT::Server server_;
    std::deque<IncomingMqttMessage> pendingCommands_;
};

#endif
```

- [ ] **Step 5: Build-check against the real target**

Run: `pio run -e esp32dev`
Expected: SUCCESS

- [ ] **Step 6: Confirm no native regressions**

Run: `pio test -e native`
Expected: PASS — unchanged suite count from Task 12

- [ ] **Step 7: Commit**

```bash
git add platformio.ini lib/Loco2MqttCore/src/adapters/PicoMqttPort.h lib/Loco2MqttCore/src/adapters/PicoMqttPort.cpp
git commit -m "feat: add PicoMQTT dependency and PicoMqttPort adapter"
```

---

### Task 15: Wire the MQTT bridge into the composition root

**Files:**
- Modify: `src/main.cpp`

**Interfaces:**
- Consumes: everything from Tasks 1-14.

**Global-constraint reminder for this task:** the entire MQTT bridge is
constructed only when `bootMode == BootMode::Normal`. `NeedsCommissioning`
and `WirelessSetup` are unchanged from the shipped commissioning
sub-project.

- [ ] **Step 1: Modify `src/main.cpp`**

```cpp
// src/main.cpp
#include <Arduino.h>
#include <optional>

#include "adapters/ArduinoClock.h"
#include "adapters/CaptivePortalServer.h"
#include "adapters/EspDigitalInput.h"
#include "adapters/EspRebootTrigger.h"
#include "adapters/EspUartPort.h"
#include "adapters/EspWifiPort.h"
#include "adapters/LocoNetEsp32Port.h"
#include "adapters/NvsConfigStore.h"
#include "adapters/NvsSetupModeRequestStore.h"
#include "adapters/PicoMqttPort.h"
#include "adapters/SerialCommissioningAdapter.h"
#include "adapters/SerialMessageLog.h"
#include "adapters/WebFormCommissioningAdapter.h"
#include "application/ButtonSetupModeTrigger.h"
#include "application/CommissioningSession.h"
#include "application/LocoNetMessageLogger.h"
#include "application/LocoNetMessageRouter.h"
#include "application/MqttCommandRouter.h"
#include "application/PendingLocoNetSendScheduler.h"
#include "domain/BootMode.h"
#include "turnout/TurnoutLocoNetDecoder.h"
#include "turnout/TurnoutLocoNetEncoder.h"
#include "turnout/TurnoutMqttCommandDecoder.h"
#include "turnout/TurnoutMqttEncoder.h"

namespace
{
    // UART2 RX — LocoNetESP32HB requires a hardware UART pin at 16.66kbps;
    // see docs/breadboard-build-guide.md's "Connecting Board A" step.
    constexpr int kLocoNetRxPin = 16;

    // Bit-banged TX into the 2N3904 open-collector driver. GPIO17 is not
    // one of the ESP32's documented strapping pins (0, 2, 5, 12, 15) and is
    // not driven by the ROM bootloader, so it should not glitch high across
    // reset and pull the shared LocoNet bus low. Verify this against your
    // specific dev board and with a scope through a reset cycle before
    // connecting to a live bus — see README.md "Hardware configuration".
    constexpr int kLocoNetTxPin = 17;

    // The ESP32's BOOT button, wired active-low with an internal pull-up.
    constexpr int kBootButtonPin = 0;

    constexpr unsigned long kSerialBaudRate = 115200;

    BootMode bootMode = BootMode::Normal;
}

NvsConfigStore configStore;
NvsSetupModeRequestStore setupModeRequestStore;
ArduinoClock systemClock;
EspDigitalInput bootButton(kBootButtonPin);
EspUartPort uartPort;
EspRebootTrigger rebootTrigger;
SerialMessageLog messageLog;

std::optional<LocoNetEsp32Port> locoNetPort;
std::optional<LocoNetMessageLogger> logger;
std::optional<ButtonSetupModeTrigger> setupModeTrigger;
std::optional<CommissioningSession> commissioningSession;
std::optional<SerialCommissioningAdapter> serialCommissioning;
std::optional<WebFormCommissioningAdapter> webFormAdapter;
std::optional<CaptivePortalServer> captivePortal;

std::optional<EspWifiPort> wifiPort;
std::optional<PicoMqttPort> mqttPort;
std::optional<PendingLocoNetSendScheduler> sendScheduler;
std::optional<TurnoutLocoNetDecoder> turnoutLocoNetDecoder;
std::optional<TurnoutMqttEncoder> turnoutMqttEncoder;
std::optional<TurnoutMqttCommandDecoder> turnoutMqttCommandDecoder;
std::optional<TurnoutLocoNetEncoder> turnoutLocoNetEncoder;
std::optional<LocoNetMessageRouter> locoNetMessageRouter;
std::optional<MqttCommandRouter> mqttCommandRouter;

namespace
{
    void setupMqttBridge(const LocoNetAdapterConfig& config)
    {
        wifiPort.emplace(config.wifiSsid(), config.wifiPassword());
        mqttPort.emplace();
        sendScheduler.emplace(*locoNetPort, systemClock);
        turnoutLocoNetDecoder.emplace();
        turnoutMqttEncoder.emplace();
        turnoutMqttCommandDecoder.emplace();
        turnoutLocoNetEncoder.emplace();
        locoNetMessageRouter.emplace(*locoNetPort, *mqttPort, systemClock,
                                      std::vector<std::pair<LocoNetMessageDecoder*, MqttEventEncoder*>>{
                                          {&*turnoutLocoNetDecoder, &*turnoutMqttEncoder}});
        mqttCommandRouter.emplace(*mqttPort, *sendScheduler,
                                   std::vector<std::pair<MqttCommandDecoder*, LocoNetEncoder*>>{
                                       {&*turnoutMqttCommandDecoder, &*turnoutLocoNetEncoder}});
        mqttPort->begin();
    }

    void setupNormalOrNeedsCommissioning()
    {
        locoNetPort.emplace(kLocoNetRxPin, kLocoNetTxPin);
        logger.emplace(*locoNetPort, messageLog);
        setupModeTrigger.emplace(bootButton, systemClock, setupModeRequestStore);
        if (bootMode == BootMode::NeedsCommissioning)
        {
            commissioningSession.emplace(configStore);
            serialCommissioning.emplace(uartPort, *commissioningSession);
            return;
        }
        setupMqttBridge(configStore.load());
    }

    void setupWirelessSetup()
    {
        webFormAdapter.emplace(configStore, rebootTrigger);
        captivePortal.emplace(*webFormAdapter);
        captivePortal->begin();
    }
}

void setup()
{
    Serial.begin(kSerialBaudRate);
    bootMode = selectBootMode(configStore.load(), setupModeRequestStore.consumeIfRequested());
    if (bootMode == BootMode::WirelessSetup)
    {
        setupWirelessSetup();
    }
    else
    {
        setupNormalOrNeedsCommissioning();
    }
}

void loop()
{
    if (bootMode == BootMode::WirelessSetup)
    {
        captivePortal->update();
        return;
    }
    locoNetPort->update();
    logger->update();
    if (setupModeTrigger->update())
    {
        ESP.restart();
    }
    if (bootMode == BootMode::NeedsCommissioning)
    {
        serialCommissioning->update();
        return;
    }
    wifiPort->update();
    mqttPort->update();
    sendScheduler->update();
    locoNetMessageRouter->update();
    mqttCommandRouter->update();
}
```

**Note on `setupMqttBridge`'s length:** this is a pure sequential
construction/wiring function — every line either constructs one object or
registers it with the next, no branching. It exceeds 8 lines because
`BootMode::Normal`'s wiring genuinely has this many parts, matching the
existing plan-mandated exemption for pure dispatch/wiring chains (see
Global Constraints) rather than nested logic that should be split.

- [ ] **Step 2: Build-check against the real target**

Run: `pio run -e esp32dev`
Expected: SUCCESS

- [ ] **Step 3: Confirm no native regressions**

Run: `pio test -e native`
Expected: PASS — unchanged suite count from Task 12 (nothing in
`src/main.cpp` is native-tested; `test_build_src = false` means this file
is never compiled under `native`)

- [ ] **Step 4: Commit**

```bash
git add src/main.cpp
git commit -m "feat: wire the MQTT turnout bridge into the composition root"
```

---

### Task 16: Update README.md and CLAUDE.md

**Files:**
- Modify: `README.md`
- Modify: `CLAUDE.md`

**Interfaces:** none — documentation only.

- [ ] **Step 1: Update `README.md`**

- Change the top status line and "What it does today"/"What it doesn't do
  yet" sections to state: the firmware now connects to WiFi using stored
  commissioning credentials, runs PicoMQTT on-device as the broker, and
  bridges turnout traffic both directions; MQTT bridging for any other
  device type (sensor, transponder, etc.) remains future work.
- Update the native test count (20 suites as of the commissioning
  sub-project → new total after this plan's 12 new suites — count the
  actual `test/test_*` directories at the time this task is executed and
  use the real number, the same way the commissioning sub-project's
  README update did).
- Add a new "MQTT turnout bridge" section (parallel to the existing "WiFi
  commissioning" section) documenting: the `loconet/turnout/<address>/state`
  and `.../set` topics, the payload values, and the note that PicoMQTT's
  broker mode doesn't honor retained/QoS so state is periodically
  re-published instead (every 30s) for late subscribers.
- Update the "Architecture overview" file tree to add `turnout/` and the
  new `domain/`/`ports/`/`application/`/`adapters/` files from this plan,
  matching the style of the existing tree.
- Add a "Known limitations" bullet: turnout is the only device type
  bridged; sensor/transponder/etc. are not yet implemented.

- [ ] **Step 2: Update `CLAUDE.md`**

Add to "Current source layout":
- `lib/Loco2MqttCore/src/domain/` — append `TurnoutAddress`,
  `TurnoutPosition`, `TurnoutStateChanged`, `SetTurnoutPosition`,
  `DomainEvent`, `DomainCommand`, `MqttMessage`, `IncomingMqttMessage`,
  `PendingLocoNetSend`, `LocoNetChecksum`.
- `lib/Loco2MqttCore/src/ports/` — append `MqttPort`,
  `LocoNetSendScheduler`, `LocoNetMessageDecoder`, `MqttEventEncoder`,
  `MqttCommandDecoder`, `LocoNetEncoder`.
- `lib/Loco2MqttCore/src/application/` — append
  `PendingLocoNetSendScheduler`, `LocoNetMessageRouter`,
  `MqttCommandRouter`.
- `lib/Loco2MqttCore/src/turnout/` — new bullet:
  `TurnoutLocoNetDecoder`, `TurnoutMqttEncoder`,
  `TurnoutMqttCommandDecoder`, `TurnoutLocoNetEncoder`.
- `lib/Loco2MqttCore/src/adapters/` — append `EspWifiPort`,
  `PicoMqttPort`.
- `test/support/` — append `FakeMqttPort`, `FakeLocoNetSendScheduler`.

Also fix the pre-existing stale note about `BootModeSelector` if it has
not already been corrected (it should read "`selectBootMode` is a free
function, not a class").

- [ ] **Step 3: Verify no code changes are needed**

Run: `pio test -e native` and `pio run -e esp32dev`
Expected: both unchanged/PASS — this task is documentation-only

- [ ] **Step 4: Commit**

```bash
git add README.md CLAUDE.md
git commit -m "docs: document the MQTT turnout bridge"
```
