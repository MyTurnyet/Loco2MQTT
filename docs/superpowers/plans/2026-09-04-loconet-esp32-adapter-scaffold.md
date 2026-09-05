# LocoNet ESP32 Adapter — Scaffold + First Vertical Slice Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Stand up a PlatformIO project for the ESP32 LocoNet adapter firmware, with hexagonal architecture, a native Catch2 test harness, and one real TDD'd vertical slice — "log every received LocoNet message" — wired end to end on real hardware.

**Architecture:** Hexagonal (ports & adapters), modeled directly on the user's existing `D:\Development\MaltbeeTurnoutController` and `D:\Development\MaltbeeController` projects. Domain/application code lives in `lib/Loco2MqttCore/src/{domain,ports,application}` and is compiled and tested under a host-native PlatformIO environment with zero Arduino dependency. Hardware-facing adapters live in `lib/Loco2MqttCore/src/adapters`, each `#ifdef ARDUINO`-guarded, and are the only files allowed to include `Arduino.h` or the `LocoNetESP32HB` library. `src/main.cpp` is a thin composition root.

**Tech Stack:** PlatformIO, Arduino framework (`espressif32`, board `esp32dev`), C++17, Catch2 3.7.1 (vendored, host-native tests, `test_framework = custom`), `LocoNetESP32HB` (a.k.a. `IoTT_LocoNetHBESP32`, by tanner87661) pinned to commit `959207d3f82c49356bcf370940d590cad47afb19` (no tagged releases exist upstream).

## Global Constraints

- Strict TDD: every behavior-bearing class gets a failing native test written and run (and confirmed failing) before the implementation that makes it pass.
- No mocking frameworks. Test doubles are hand-written fakes in `test/support/`, one per port.
- Domain and application code depend only on interfaces (`ports/`) they define — never on `Arduino.h`, `LocoNetESP32HB`, or any ESP32 SDK type, directly or transitively. This must hold true under compilation, not just by convention: the `native` environment builds and tests this code with zero Arduino headers on the include path.
- Composition over inheritance; adapters compose owned library/hardware objects rather than subclassing them beyond the one required port interface.
- Ask, don't tell: ports expose behavior (`receive()`, `record()`, `write()`), not raw state a caller would branch on. Getters on domain value objects are fine when they hand back the object's own data for a legitimate external need (e.g. serializing to hardware, or a test assertion) — the rule is about not leaking decision-making, not about hiding data.
- Immutable by default. `LocoNetMessage` and `Level` are immutable value types. Mutable state is confined to adapters that own real hardware state (e.g. a receive queue fed by a hardware callback) and to non-const `update()`/`poll()` cycling.
- No statics or globals in domain/application/adapter code. `src/main.cpp` is the sole exception — Arduino's `setup()`/`loop()` model has no other place to hold constructed objects across calls, so file-scope object construction in `main.cpp` is the accepted composition-root idiom (confirmed by both reference projects' `main.cpp`). The one narrow exception inside an adapter: `LocoNetEsp32Port`'s translation-unit-local callback bridge (Task 7) — required because the vendor library's callback is a plain C function pointer with no user-data parameter, exactly the same constraint the user's existing `MrrwaLocoNetFeedbackSource.cpp` already works around the same way.
- Methods ≤ 8 lines; cognitive complexity < 4. Any method threatening to exceed this gets split (see `LocoNetMessage::describe()`'s helper function in Task 4).
- Two PlatformIO environments only: `esp32dev` (real hardware, `framework = arduino`) and `native` (host-only Catch2 tests, no hardware).
- `InverseLogic` is hardcoded `true` in the one place that constructs the real LocoNet adapter — this circuit's opto output is always inverted, so it is not a runtime-configurable option (YAGNI).
- The TX GPIO must be boot-safe: it must never be observed high across an ESP32 reset, since that would turn on the 2N3904 and pull the shared LocoNet bus low. Firmware cannot fully control silicon-level reset behavior before `setup()` runs — the primary safeguard is *which pin is chosen* (avoiding the ESP32's documented strapping pins: GPIO0, 2, 5, 12, 15), documented plainly in README.md, plus verifying with a scope through a reset cycle per the build guide's own troubleshooting section.

---

## File Structure

```
Loco2MQTT/
├── platformio.ini
├── CLAUDE.md
├── README.md
├── .gitignore
├── docs/
│   └── breadboard-build-guide.md
├── src/
│   └── main.cpp                                    # composition root
├── include/
│   └── README
├── lib/
│   ├── README
│   ├── Catch2/                                     # vendored 3.7.1, copied as-is
│   └── Loco2MqttCore/src/
│       ├── domain/
│       │   ├── Level.h
│       │   ├── LocoNetMessage.h
│       │   └── LocoNetMessage.cpp
│       ├── ports/
│       │   ├── DigitalPin.h
│       │   ├── LocoNetPort.h
│       │   └── MessageLog.h
│       ├── application/
│       │   ├── LocoNetMessageLogger.h
│       │   └── LocoNetMessageLogger.cpp
│       └── adapters/
│           ├── EspDigitalPin.h                     # #ifdef ARDUINO
│           ├── LocoNetEsp32Port.h                  # #ifdef ARDUINO
│           ├── LocoNetEsp32Port.cpp                # #ifdef ARDUINO
│           └── SerialMessageLog.h                  # #ifdef ARDUINO
└── test/
    ├── README
    ├── test_custom_runner.py                       # copied as-is
    ├── support/
    │   ├── FakeDigitalPin.h
    │   ├── FakeLocoNetPort.h
    │   └── FakeMessageLog.h
    ├── test_example/test_main.cpp
    ├── test_fake_digital_pin/test_main.cpp
    ├── test_loco_net_message/test_main.cpp
    ├── test_fake_loco_net_port/test_main.cpp
    ├── test_fake_message_log/test_main.cpp
    └── test_loco_net_message_logger/test_main.cpp
```

`lib/Loco2MqttCore` is one library folder (mirroring `MaltbeeTurnoutController`'s single-target `McsCore` layout — this project, like that one, targets exactly one board, so there is no need for the multi-lib `McsCore`/`McsEsp32`/`McsLoconet` split `MaltbeeController` uses for its dual Mega/ESP32 targets).

---

### Task 1: Project scaffold + native test harness proof

**Files:**
- Create: `platformio.ini`
- Create: `.gitignore`
- Create: `include/README`
- Create: `lib/README`
- Create: `test/README`
- Create: `docs/breadboard-build-guide.md`
- Create: `src/main.cpp` (placeholder — empty `setup()`/`loop()`)
- Create: `test/test_example/test_main.cpp`
- Copy: `lib/Catch2/` (from `D:\Development\MaltbeeTurnoutController\lib\Catch2`)
- Copy: `test/test_custom_runner.py` (from `D:\Development\MaltbeeTurnoutController\test\test_custom_runner.py`)

**Interfaces:** None yet — this task only proves the build/test toolchain works.

- [ ] **Step 1: Initialize the git repository**

```bash
cd "D:/Development/Loco2MQTT"
git init
```

- [ ] **Step 2: Vendor Catch2 and the custom test runner**

```bash
cp -r "D:/Development/MaltbeeTurnoutController/lib/Catch2" "D:/Development/Loco2MQTT/lib/Catch2"
cp "D:/Development/MaltbeeTurnoutController/test/test_custom_runner.py" "D:/Development/Loco2MQTT/test/test_custom_runner.py"
```

- [ ] **Step 3: Write `.gitignore`**

```gitignore
# PlatformIO build output
.pio/

# PlatformIO generated project metadata
.pioenvs/
.piolibdeps/

# CLion / JetBrains project files
.idea/
cmake-build-*/

# CMake generated files
CMakeFiles/
CMakeCache.txt
cmake_install.cmake
Makefile

# Compiled binaries and object files
*.o
*.obj
*.a
*.lib
*.elf
*.hex
*.bin
*.exe
*.out

# Dependency files
*.d

# Debug files
*.map
*.lst

# Test output and coverage
coverage/
*.gcda
*.gcno
*.gcov

# Logs
*.log

# Operating system files
.DS_Store
Thumbs.db
desktop.ini

# Editor swap and temporary files
*.swp
*.swo
*~
*.tmp
*.temp

# Visual Studio Code settings, if used later
.vscode/

# Python cache, if helper scripts are added later
__pycache__/
*.py[cod]

# Keep empty project directories if they contain .gitkeep files
!.gitkeep
```

- [ ] **Step 4: Write `platformio.ini`**

```ini
[platformio]
default_envs = esp32dev

[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200
lib_ldf_mode = deep+
build_unflags = -std=gnu++11
build_flags = -std=gnu++17

[env:native]
platform = native
test_framework = custom
test_build_src = false
build_flags = -std=c++17 -Ilib/Loco2MqttCore/src
```

- [ ] **Step 5: Write placeholder README files**

`include/README`:
```
This directory is intended for project header files.

See https://docs.platformio.org/page/projectconf/section_env_build.html#include-dir
```

`lib/README`:
```
This directory is intended for project specific (private) libraries.
PlatformIO will compile them to static libraries and link them into executable file.

See https://docs.platformio.org/page/librarymanager/creating.html
```

`test/README`:
```
This directory is intended for PlatformIO Test Runner and project tests.

See https://docs.platformio.org/page/plus/unit-testing.html
```

- [ ] **Step 6: Save the hardware build guide into the repo**

Create `docs/breadboard-build-guide.md` containing the full text the user pasted (the "ESP32 LocoNet Adapter — Breadboard Build Guide" document, verbatim, starting with `# ESP32 LocoNet Adapter — Breadboard Build Guide` and ending with the "Notes for the PCB phase (later)" section). This is the source-of-truth reference for RX/TX pin behavior, `InverseLogic`, and connector wiring that later tasks and the README point back to.

- [ ] **Step 7: Write the placeholder composition root**

`src/main.cpp`:
```cpp
#include <Arduino.h>

void setup()
{
}

void loop()
{
}
```

- [ ] **Step 8: Write one trivial passing native test to prove the harness**

`test/test_example/test_main.cpp`:
```cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

TEST_CASE("The native Catch2 test harness runs")
{
    REQUIRE(1 + 1 == 2);
}
```

- [ ] **Step 9: Run the native tests and confirm they pass**

Run: `pio test -e native`
Expected: `test_example` reports 1 test case, 1 assertion, PASSED. (If it builds but the binary fails to launch with a Windows `0xC0000139` status, your MinGW `bin` directory is shadowed on `PATH` by another GCC/MinGW install — put MinGW's `bin` first.)

- [ ] **Step 10: Confirm the esp32dev target builds**

Run: `pio run -e esp32dev`
Expected: `SUCCESS` — an empty `setup()`/`loop()` sketch builds cleanly against the Arduino framework.

- [ ] **Step 11: Commit**

```bash
git add .gitignore platformio.ini include lib/README lib/Catch2 test/README test/test_custom_runner.py test/test_example src docs
git commit -m "chore: scaffold PlatformIO project with native Catch2 harness"
```

---

### Task 2: `Level` + `DigitalPin` port + `FakeDigitalPin`

**Files:**
- Create: `lib/Loco2MqttCore/src/domain/Level.h`
- Create: `lib/Loco2MqttCore/src/ports/DigitalPin.h`
- Create: `test/support/FakeDigitalPin.h`
- Test: `test/test_fake_digital_pin/test_main.cpp`

**Interfaces:**
- Produces: `enum class Level { Low, High }`; `class DigitalPin { virtual void write(Level) = 0; }`; `class FakeDigitalPin : public DigitalPin` with `Level level() const` and `int writeCallCount() const`.

This is the general-purpose GPIO seam requested alongside `LocoNetPort` — not consumed by the logging vertical slice (the `LocoNetESP32HB` library owns its own RX/TX pins internally), but scaffolded now per spec, the same way the reference projects scaffold ports ahead of their first consumer.

- [ ] **Step 1: Write the failing test**

`test/test_fake_digital_pin/test_main.cpp`:
```cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "support/FakeDigitalPin.h"

TEST_CASE("FakeDigitalPin begins Low")
{
    FakeDigitalPin pin;

    REQUIRE(pin.level() == Level::Low);
}

TEST_CASE("write(High) reports High")
{
    FakeDigitalPin pin;

    pin.write(Level::High);

    REQUIRE(pin.level() == Level::High);
}

TEST_CASE("write(Low) reports Low")
{
    FakeDigitalPin pin;
    pin.write(Level::High);

    pin.write(Level::Low);

    REQUIRE(pin.level() == Level::Low);
}

TEST_CASE("write() records how many times it was called")
{
    FakeDigitalPin pin;

    pin.write(Level::High);
    pin.write(Level::Low);
    pin.write(Level::High);

    REQUIRE(pin.writeCallCount() == 3);
}
```

- [ ] **Step 2: Run the test and confirm it fails to compile**

Run: `pio test -e native -f test_fake_digital_pin`
Expected: FAIL — `support/FakeDigitalPin.h: No such file or directory` (nothing exists yet).

- [ ] **Step 3: Write `Level`**

`lib/Loco2MqttCore/src/domain/Level.h`:
```cpp
#pragma once

enum class Level
{
    Low,
    High
};
```

- [ ] **Step 4: Write the `DigitalPin` port**

`lib/Loco2MqttCore/src/ports/DigitalPin.h`:
```cpp
#pragma once

#include "domain/Level.h"

class DigitalPin
{
public:
    virtual ~DigitalPin() = default;
    virtual void write(Level level) = 0;
};
```

- [ ] **Step 5: Write `FakeDigitalPin`**

`test/support/FakeDigitalPin.h`:
```cpp
#pragma once

#include "ports/DigitalPin.h"

class FakeDigitalPin : public DigitalPin
{
public:
    void write(Level level) override
    {
        level_ = level;
        writeCallCount_++;
    }

    Level level() const
    {
        return level_;
    }

    int writeCallCount() const
    {
        return writeCallCount_;
    }

private:
    Level level_ = Level::Low;
    int writeCallCount_ = 0;
};
```

- [ ] **Step 6: Run the test and confirm it passes**

Run: `pio test -e native -f test_fake_digital_pin`
Expected: 4 test cases, all PASSED.

- [ ] **Step 7: Commit**

```bash
git add lib/Loco2MqttCore/src/domain/Level.h lib/Loco2MqttCore/src/ports/DigitalPin.h test/support/FakeDigitalPin.h test/test_fake_digital_pin
git commit -m "feat: add Level, DigitalPin port, and FakeDigitalPin"
```

---

### Task 3: `EspDigitalPin` adapter (build-check only)

**Files:**
- Create: `lib/Loco2MqttCore/src/adapters/EspDigitalPin.h`
- Modify: `src/main.cpp` (temporarily, reverted at the end of this task)

**Interfaces:**
- Consumes: `DigitalPin` (Task 2), `Level` (Task 2).
- Produces: `class EspDigitalPin final : public DigitalPin`, constructed as `EspDigitalPin(int pin)`.

No native test is possible here: like the reference projects' `ArduinoDigitalOutput`, this class's body is entirely `#ifdef ARDUINO`-guarded and calls real `pinMode`/`digitalWrite`, so it compiles to nothing under `native`. Verification is a temporary build-check in `main.cpp`, then revert — the same pattern the reference projects use for `LedPairStation`/`NvsConfigStore`.

- [ ] **Step 1: Write the adapter**

`lib/Loco2MqttCore/src/adapters/EspDigitalPin.h`:
```cpp
#pragma once

#ifdef ARDUINO

#include <Arduino.h>

#include "ports/DigitalPin.h"

class EspDigitalPin final : public DigitalPin
{
public:
    explicit EspDigitalPin(int pin) : pin_(pin)
    {
        pinMode(pin_, OUTPUT);
        digitalWrite(pin_, LOW);
    }

    void write(Level level) override
    {
        digitalWrite(pin_, level == Level::High ? HIGH : LOW);
    }

private:
    int pin_;
};

#endif
```

The constructor drives the pin low the instant our code takes ownership of it — the best firmware-level guarantee available. It cannot fix the ESP32's own silicon reset/strapping behavior before `setup()` runs; that is a pin-choice concern (see README.md).

- [ ] **Step 2: Temporarily build-check against real hardware**

Edit `src/main.cpp` to temporarily read:
```cpp
#include <Arduino.h>

#include "adapters/EspDigitalPin.h"

void setup()
{
    EspDigitalPin buildCheckPin(4);
    buildCheckPin.write(Level::High);
}

void loop()
{
}
```

Run: `pio run -e esp32dev`
Expected: `SUCCESS`.

- [ ] **Step 3: Revert `src/main.cpp`**

Restore it to the empty placeholder from Task 1, Step 7:
```cpp
#include <Arduino.h>

void setup()
{
}

void loop()
{
}
```

- [ ] **Step 4: Confirm native tests are unaffected**

Run: `pio test -e native`
Expected: same pass count as the end of Task 2 (this task added no native-testable code).

- [ ] **Step 5: Commit**

```bash
git add lib/Loco2MqttCore/src/adapters/EspDigitalPin.h
git commit -m "feat: add EspDigitalPin adapter, build-checked against esp32dev"
```

---

### Task 4: `LocoNetMessage` domain value object

**Files:**
- Create: `lib/Loco2MqttCore/src/domain/LocoNetMessage.h`
- Create: `lib/Loco2MqttCore/src/domain/LocoNetMessage.cpp`
- Test: `test/test_loco_net_message/test_main.cpp`

**Interfaces:**
- Produces: `class LocoNetMessage` — constructed from `std::vector<uint8_t>`, with `bytes()`, `describe()`, `operator==`/`operator!=`.

`bytes()` exists because a real adapter (Task 7) must serialize the message onto the wire, and `describe()` exists so any `MessageLog` can render a human-readable line without knowing the byte layout itself — the log asks the message to describe itself rather than reformatting raw bytes itself.

- [ ] **Step 1: Write the failing test**

`test/test_loco_net_message/test_main.cpp`:
```cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/LocoNetMessage.h"

TEST_CASE("describe() renders an empty message as an empty string")
{
    LocoNetMessage message({});

    REQUIRE(message.describe() == "");
}

TEST_CASE("describe() renders a single byte as two uppercase hex digits")
{
    LocoNetMessage message({0x0A});

    REQUIRE(message.describe() == "0A");
}

TEST_CASE("describe() renders multiple bytes space-separated")
{
    LocoNetMessage message({0xB2, 0x00, 0x00, 0x50});

    REQUIRE(message.describe() == "B2 00 00 50");
}

TEST_CASE("bytes() returns the original bytes")
{
    LocoNetMessage message({0xB2, 0x00});

    REQUIRE(message.bytes() == std::vector<uint8_t>{0xB2, 0x00});
}

TEST_CASE("Messages with equal bytes compare equal")
{
    LocoNetMessage a({0xB2, 0x00});
    LocoNetMessage b({0xB2, 0x00});

    REQUIRE(a == b);
}

TEST_CASE("Messages with different bytes compare unequal")
{
    LocoNetMessage a({0xB2, 0x00});
    LocoNetMessage b({0xB2, 0x01});

    REQUIRE(a != b);
}
```

- [ ] **Step 2: Run the test and confirm it fails to compile**

Run: `pio test -e native -f test_loco_net_message`
Expected: FAIL — `domain/LocoNetMessage.h: No such file or directory`.

- [ ] **Step 3: Write the header**

`lib/Loco2MqttCore/src/domain/LocoNetMessage.h`:
```cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

class LocoNetMessage
{
public:
    explicit LocoNetMessage(std::vector<uint8_t> bytes) : bytes_(std::move(bytes))
    {
    }

    const std::vector<uint8_t>& bytes() const
    {
        return bytes_;
    }

    std::string describe() const;

    bool operator==(const LocoNetMessage& other) const
    {
        return bytes_ == other.bytes_;
    }

    bool operator!=(const LocoNetMessage& other) const
    {
        return !(*this == other);
    }

private:
    std::vector<uint8_t> bytes_;
};
```

- [ ] **Step 4: Write the implementation**

`lib/Loco2MqttCore/src/domain/LocoNetMessage.cpp`:
```cpp
#include "LocoNetMessage.h"

#include <iomanip>
#include <sstream>

namespace
{
    std::string byteToHex(uint8_t value)
    {
        std::ostringstream out;
        out << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << static_cast<int>(value);
        return out.str();
    }
}

std::string LocoNetMessage::describe() const
{
    std::string result;
    for (size_t i = 0; i < bytes_.size(); ++i)
    {
        result += (i > 0 ? " " : "") + byteToHex(bytes_[i]);
    }
    return result;
}
```

- [ ] **Step 5: Run the test and confirm it passes**

Run: `pio test -e native -f test_loco_net_message`
Expected: 6 test cases, all PASSED.

- [ ] **Step 6: Commit**

```bash
git add lib/Loco2MqttCore/src/domain/LocoNetMessage.h lib/Loco2MqttCore/src/domain/LocoNetMessage.cpp test/test_loco_net_message
git commit -m "feat: add LocoNetMessage domain value object"
```

---

### Task 5: `LocoNetPort` + `MessageLog` ports and their fakes

**Files:**
- Create: `lib/Loco2MqttCore/src/ports/LocoNetPort.h`
- Create: `lib/Loco2MqttCore/src/ports/MessageLog.h`
- Create: `test/support/FakeLocoNetPort.h`
- Create: `test/support/FakeMessageLog.h`
- Test: `test/test_fake_loco_net_port/test_main.cpp`
- Test: `test/test_fake_message_log/test_main.cpp`

**Interfaces:**
- Consumes: `LocoNetMessage` (Task 4).
- Produces: `class LocoNetPort { std::optional<LocoNetMessage> receive(); void send(const LocoNetMessage&); }`; `class MessageLog { void record(const LocoNetMessage&); }`; `FakeLocoNetPort` with `enqueue(const LocoNetMessage&)` and `sent()`; `FakeMessageLog` with `recorded()`.

`receive()` returns `std::optional<LocoNetMessage>` rather than a bool-plus-out-param — that keeps `LocoNetMessage` genuinely immutable (no default constructor needed just to satisfy an out-parameter).

- [ ] **Step 1: Write the failing tests**

`test/test_fake_loco_net_port/test_main.cpp`:
```cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "support/FakeLocoNetPort.h"

TEST_CASE("receive() returns nullopt when nothing was enqueued")
{
    FakeLocoNetPort port;

    REQUIRE(port.receive() == std::nullopt);
}

TEST_CASE("receive() returns an enqueued message")
{
    FakeLocoNetPort port;
    LocoNetMessage message({0xB2, 0x00});
    port.enqueue(message);

    REQUIRE(port.receive() == message);
}

TEST_CASE("receive() returns enqueued messages in FIFO order, then nullopt")
{
    FakeLocoNetPort port;
    port.enqueue(LocoNetMessage({0x01}));
    port.enqueue(LocoNetMessage({0x02}));

    REQUIRE(port.receive() == LocoNetMessage({0x01}));
    REQUIRE(port.receive() == LocoNetMessage({0x02}));
    REQUIRE(port.receive() == std::nullopt);
}

TEST_CASE("send() records the sent message")
{
    FakeLocoNetPort port;
    LocoNetMessage message({0xAA});

    port.send(message);

    REQUIRE(port.sent().size() == 1);
    REQUIRE(port.sent()[0] == message);
}
```

`test/test_fake_message_log/test_main.cpp`:
```cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "support/FakeMessageLog.h"

TEST_CASE("A fresh FakeMessageLog has recorded nothing")
{
    FakeMessageLog log;

    REQUIRE(log.recorded().empty());
}

TEST_CASE("record() captures the message")
{
    FakeMessageLog log;
    LocoNetMessage message({0xB2, 0x00});

    log.record(message);

    REQUIRE(log.recorded().size() == 1);
    REQUIRE(log.recorded()[0] == message);
}
```

- [ ] **Step 2: Run the tests and confirm they fail to compile**

Run: `pio test -e native -f test_fake_loco_net_port -f test_fake_message_log`
Expected: FAIL — `support/FakeLocoNetPort.h: No such file or directory` (and likewise for `FakeMessageLog.h`).

- [ ] **Step 3: Write the `LocoNetPort` port**

`lib/Loco2MqttCore/src/ports/LocoNetPort.h`:
```cpp
#pragma once

#include <optional>

#include "domain/LocoNetMessage.h"

class LocoNetPort
{
public:
    virtual ~LocoNetPort() = default;
    virtual std::optional<LocoNetMessage> receive() = 0;
    virtual void send(const LocoNetMessage& message) = 0;
};
```

- [ ] **Step 4: Write the `MessageLog` port**

`lib/Loco2MqttCore/src/ports/MessageLog.h`:
```cpp
#pragma once

#include "domain/LocoNetMessage.h"

class MessageLog
{
public:
    virtual ~MessageLog() = default;
    virtual void record(const LocoNetMessage& message) = 0;
};
```

- [ ] **Step 5: Write `FakeLocoNetPort`**

`test/support/FakeLocoNetPort.h`:
```cpp
#pragma once

#include <deque>
#include <vector>

#include "ports/LocoNetPort.h"

class FakeLocoNetPort : public LocoNetPort
{
public:
    void enqueue(const LocoNetMessage& message)
    {
        toReceive_.push_back(message);
    }

    std::optional<LocoNetMessage> receive() override
    {
        if (toReceive_.empty())
        {
            return std::nullopt;
        }
        LocoNetMessage message = toReceive_.front();
        toReceive_.pop_front();
        return message;
    }

    void send(const LocoNetMessage& message) override
    {
        sent_.push_back(message);
    }

    const std::vector<LocoNetMessage>& sent() const
    {
        return sent_;
    }

private:
    std::deque<LocoNetMessage> toReceive_;
    std::vector<LocoNetMessage> sent_;
};
```

- [ ] **Step 6: Write `FakeMessageLog`**

`test/support/FakeMessageLog.h`:
```cpp
#pragma once

#include <vector>

#include "ports/MessageLog.h"

class FakeMessageLog : public MessageLog
{
public:
    void record(const LocoNetMessage& message) override
    {
        recorded_.push_back(message);
    }

    const std::vector<LocoNetMessage>& recorded() const
    {
        return recorded_;
    }

private:
    std::vector<LocoNetMessage> recorded_;
};
```

- [ ] **Step 7: Run the tests and confirm they pass**

Run: `pio test -e native -f test_fake_loco_net_port -f test_fake_message_log`
Expected: 4 + 2 test cases, all PASSED.

- [ ] **Step 8: Commit**

```bash
git add lib/Loco2MqttCore/src/ports/LocoNetPort.h lib/Loco2MqttCore/src/ports/MessageLog.h test/support/FakeLocoNetPort.h test/support/FakeMessageLog.h test/test_fake_loco_net_port test/test_fake_message_log
git commit -m "feat: add LocoNetPort and MessageLog ports with fakes"
```

---

### Task 6: `LocoNetMessageLogger` application service (the vertical slice's domain logic)

**Files:**
- Create: `lib/Loco2MqttCore/src/application/LocoNetMessageLogger.h`
- Create: `lib/Loco2MqttCore/src/application/LocoNetMessageLogger.cpp`
- Test: `test/test_loco_net_message_logger/test_main.cpp`

**Interfaces:**
- Consumes: `LocoNetPort`, `MessageLog` (Task 5); `FakeLocoNetPort`, `FakeMessageLog` (Task 5).
- Produces: `class LocoNetMessageLogger { LocoNetMessageLogger(LocoNetPort&, MessageLog&); void update(); }`.

This is the class the whole slice exists to prove: on each `update()`, if the port has a message waiting, it is forwarded to the log — otherwise nothing happens. Fully covered by native tests using the Task 5 fakes; no hardware involved.

- [ ] **Step 1: Write the failing tests**

`test/test_loco_net_message_logger/test_main.cpp`:
```cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "application/LocoNetMessageLogger.h"
#include "support/FakeLocoNetPort.h"
#include "support/FakeMessageLog.h"

TEST_CASE("update() does nothing when the port has no message")
{
    FakeLocoNetPort port;
    FakeMessageLog log;
    LocoNetMessageLogger logger(port, log);

    logger.update();

    REQUIRE(log.recorded().empty());
}

TEST_CASE("update() forwards a received message to the log")
{
    FakeLocoNetPort port;
    FakeMessageLog log;
    LocoNetMessageLogger logger(port, log);
    LocoNetMessage message({0xB2, 0x00, 0x00, 0x50});
    port.enqueue(message);

    logger.update();

    REQUIRE(log.recorded().size() == 1);
    REQUIRE(log.recorded()[0] == message);
}

TEST_CASE("update() forwards exactly one message per call")
{
    FakeLocoNetPort port;
    FakeMessageLog log;
    LocoNetMessageLogger logger(port, log);
    port.enqueue(LocoNetMessage({0x01}));
    port.enqueue(LocoNetMessage({0x02}));

    logger.update();
    logger.update();

    REQUIRE(log.recorded().size() == 2);
    REQUIRE(log.recorded()[0] == LocoNetMessage({0x01}));
    REQUIRE(log.recorded()[1] == LocoNetMessage({0x02}));
}
```

- [ ] **Step 2: Run the tests and confirm they fail to compile**

Run: `pio test -e native -f test_loco_net_message_logger`
Expected: FAIL — `application/LocoNetMessageLogger.h: No such file or directory`.

- [ ] **Step 3: Write the header**

`lib/Loco2MqttCore/src/application/LocoNetMessageLogger.h`:
```cpp
#pragma once

#include "ports/LocoNetPort.h"
#include "ports/MessageLog.h"

class LocoNetMessageLogger
{
public:
    LocoNetMessageLogger(LocoNetPort& port, MessageLog& log) : port_(port), log_(log)
    {
    }

    void update();

private:
    LocoNetPort& port_;
    MessageLog& log_;
};
```

- [ ] **Step 4: Write the implementation**

`lib/Loco2MqttCore/src/application/LocoNetMessageLogger.cpp`:
```cpp
#include "LocoNetMessageLogger.h"

void LocoNetMessageLogger::update()
{
    std::optional<LocoNetMessage> message = port_.receive();
    if (message.has_value())
    {
        log_.record(*message);
    }
}
```

- [ ] **Step 5: Run the tests and confirm they pass**

Run: `pio test -e native -f test_loco_net_message_logger`
Expected: 3 test cases, all PASSED.

- [ ] **Step 6: Run the full native suite**

Run: `pio test -e native`
Expected: every test suite from Tasks 1–6 passes (`test_example`, `test_fake_digital_pin`, `test_loco_net_message`, `test_fake_loco_net_port`, `test_fake_message_log`, `test_loco_net_message_logger`).

- [ ] **Step 7: Commit**

```bash
git add lib/Loco2MqttCore/src/application/LocoNetMessageLogger.h lib/Loco2MqttCore/src/application/LocoNetMessageLogger.cpp test/test_loco_net_message_logger
git commit -m "feat: add LocoNetMessageLogger application service"
```

---

### Task 7: `LocoNetEsp32Port` adapter (build-check only)

**Files:**
- Create: `lib/Loco2MqttCore/src/adapters/LocoNetEsp32Port.h`
- Create: `lib/Loco2MqttCore/src/adapters/LocoNetEsp32Port.cpp`
- Modify: `platformio.ini` (add `lib_deps`/`lib_ldf_mode` to `[env:esp32dev]`)
- Modify: `src/main.cpp` (temporarily, reverted at the end of this task)

**Interfaces:**
- Consumes: `LocoNetPort`, `LocoNetMessage` (Task 5/4).
- Produces: `class LocoNetEsp32Port final : public LocoNetPort`, constructed as `LocoNetEsp32Port(int rxPin, int txPin)`, with `begin()` and `update()`.

Wraps `LocoNetESP32HB`'s `LocoNetESPSerial` class (header `IoTT_LocoNetHBESP32.h`). That library receives messages via a plain C callback (`typedef void (*cbFct)(lnReceiveBuffer*)`, no user-data pointer), so — exactly like the existing `MrrwaLocoNetFeedbackSource.cpp` in `MaltbeeController` — the bridge from that callback into this adapter needs one translation-unit-local queue in an anonymous namespace inside the `.cpp`. This is vendor-library glue, confined entirely to this one `#ifdef ARDUINO` file; it is not a domain/application static and does not violate the no-statics rule above. It does mean only one `LocoNetEsp32Port` may exist at a time — true for this hardware (one LocoNet bus, one adapter) and worth calling out explicitly if that ever changes.

The local callback below is named `onLocoNetMessageReceived`, not the more obvious `onLocoNetMessage` — `IoTT_LocoNetHBESP32.h` itself declares a global weak symbol `extern void onLocoNetMessage(lnReceiveBuffer*) __attribute__((weak))` (line 116), and since an anonymous namespace's members are implicitly visible in the enclosing (global) scope, naming the local callback identically makes `&onLocoNetMessage` ambiguous between the two identical-signature candidates. Confirmed by build failure during plan validation; the rename is the fix.

No native test: like `EspDigitalPin`/`ArduinoDigitalOutput`, this class only compiles under `ARDUINO`. Verified by a temporary build-check in `main.cpp`.

- [ ] **Step 1: Add the library dependency**

Edit `platformio.ini`'s `[env:esp32dev]` section to add:
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
build_unflags = -std=gnu++11
build_flags = -std=gnu++17
```

(`LocoNetESP32HB` pinned to that commit because the upstream repo has no tagged releases — `git ls-remote` confirms it is the current `master` HEAD as of this plan. `ArduinoJson` added as a direct dependency, pinned via git URL to the commit `v7.2.1` resolves to, because `LocoNetESP32HB`'s own header, `IoTT_LocoNetHBESP32.h`, `#include`s `<ArduinoJson.h>` without declaring it in its own `library.json`/`library.properties` — PlatformIO's LDF never pulls it in transitively, so the build fails on a missing header unless it's declared here explicitly. The PlatformIO Registry shorthand, e.g. `bblanchon/ArduinoJson@^7.0.0`, failed with an opaque `HTTPClientError` during plan validation; the git-URL form is confirmed to work.)

- [ ] **Step 2: Write the header**

`lib/Loco2MqttCore/src/adapters/LocoNetEsp32Port.h`:
```cpp
#pragma once

#ifdef ARDUINO

#include <IoTT_LocoNetHBESP32.h>

#include "ports/LocoNetPort.h"

class LocoNetEsp32Port final : public LocoNetPort
{
public:
    LocoNetEsp32Port(int rxPin, int txPin);

    void begin();
    void update();

    std::optional<LocoNetMessage> receive() override;
    void send(const LocoNetMessage& message) override;

private:
    LocoNetESPSerial serial_;
};

#endif
```

- [ ] **Step 3: Write the implementation**

`lib/Loco2MqttCore/src/adapters/LocoNetEsp32Port.cpp`:
```cpp
#ifdef ARDUINO

#include "LocoNetEsp32Port.h"

#include <algorithm>
#include <queue>
#include <vector>

namespace
{
    std::queue<std::vector<uint8_t>>& pendingMessages()
    {
        static std::queue<std::vector<uint8_t>> queue;
        return queue;
    }

    void onLocoNetMessageReceived(lnReceiveBuffer* buffer)
    {
        pendingMessages().emplace(buffer->lnData, buffer->lnData + buffer->lnMsgSize);
    }
}

LocoNetEsp32Port::LocoNetEsp32Port(int rxPin, int txPin)
    : serial_(rxPin, txPin, /* inverse_logic = */ true)
{
    serial_.setLNCallback(&onLocoNetMessageReceived);
}

void LocoNetEsp32Port::begin()
{
    serial_.begin();
}

void LocoNetEsp32Port::update()
{
    serial_.processLoop();
}

std::optional<LocoNetMessage> LocoNetEsp32Port::receive()
{
    if (pendingMessages().empty())
    {
        return std::nullopt;
    }
    LocoNetMessage message(pendingMessages().front());
    pendingMessages().pop();
    return message;
}

void LocoNetEsp32Port::send(const LocoNetMessage& message)
{
    lnTransmitMsg packet{};
    packet.lnMsgSize = static_cast<uint8_t>(message.bytes().size());
    std::copy(message.bytes().begin(), message.bytes().end(), packet.lnData);
    serial_.lnWriteMsg(packet);
}

#endif
```

- [ ] **Step 4: Temporarily build-check against real hardware**

Edit `src/main.cpp` to temporarily read:
```cpp
#include <Arduino.h>

#include "adapters/LocoNetEsp32Port.h"

LocoNetEsp32Port locoNetPort(16, 17);

void setup()
{
    locoNetPort.begin();
}

void loop()
{
    locoNetPort.update();
}
```

Run: `pio run -e esp32dev`
Expected: `SUCCESS`.

- [ ] **Step 5: Revert `src/main.cpp`**

Restore the empty placeholder from Task 1, Step 7.

- [ ] **Step 6: Confirm native tests are unaffected**

Run: `pio test -e native`
Expected: same pass count as the end of Task 6 (the `native` env never sees this adapter — it isn't referenced by any test, and it's entirely `#ifdef ARDUINO`-guarded regardless).

- [ ] **Step 7: Commit**

```bash
git add platformio.ini lib/Loco2MqttCore/src/adapters/LocoNetEsp32Port.h lib/Loco2MqttCore/src/adapters/LocoNetEsp32Port.cpp
git commit -m "feat: add LocoNetEsp32Port adapter, build-checked against esp32dev"
```

---

### Task 8: `SerialMessageLog` adapter (build-check only)

**Files:**
- Create: `lib/Loco2MqttCore/src/adapters/SerialMessageLog.h`
- Modify: `src/main.cpp` (temporarily, reverted at the end of this task)

**Interfaces:**
- Consumes: `MessageLog`, `LocoNetMessage` (Task 5/4).
- Produces: `class SerialMessageLog final : public MessageLog`.

- [ ] **Step 1: Write the adapter**

`lib/Loco2MqttCore/src/adapters/SerialMessageLog.h`:
```cpp
#pragma once

#ifdef ARDUINO

#include <Arduino.h>

#include "ports/MessageLog.h"

class SerialMessageLog final : public MessageLog
{
public:
    void record(const LocoNetMessage& message) override
    {
        Serial.println(message.describe().c_str());
    }
};

#endif
```

- [ ] **Step 2: Temporarily build-check against real hardware**

Edit `src/main.cpp` to temporarily read:
```cpp
#include <Arduino.h>

#include "adapters/SerialMessageLog.h"
#include "domain/LocoNetMessage.h"

SerialMessageLog messageLog;

void setup()
{
    Serial.begin(115200);
    messageLog.record(LocoNetMessage({0xB2, 0x00}));
}

void loop()
{
}
```

Run: `pio run -e esp32dev`
Expected: `SUCCESS`.

- [ ] **Step 3: Revert `src/main.cpp`**

Restore the empty placeholder from Task 1, Step 7.

- [ ] **Step 4: Confirm native tests are unaffected**

Run: `pio test -e native`
Expected: same pass count as the end of Task 6.

- [ ] **Step 5: Commit**

```bash
git add lib/Loco2MqttCore/src/adapters/SerialMessageLog.h
git commit -m "feat: add SerialMessageLog adapter, build-checked against esp32dev"
```

---

### Task 9: Composition root, full hardware build, and documentation

**Files:**
- Modify: `src/main.cpp` (final, real wiring — no longer a temporary stub)
- Create: `README.md`
- Create: `CLAUDE.md`

**Interfaces:**
- Consumes: `LocoNetEsp32Port` (Task 7), `SerialMessageLog` (Task 8), `LocoNetMessageLogger` (Task 6).

This is the only task that wires real adapters together. `main.cpp` contains no business logic — it constructs three objects and calls two methods per loop tick.

- [ ] **Step 1: Write the real composition root**

`src/main.cpp`:
```cpp
#include <Arduino.h>

#include "adapters/LocoNetEsp32Port.h"
#include "adapters/SerialMessageLog.h"
#include "application/LocoNetMessageLogger.h"

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

    constexpr unsigned long kSerialBaudRate = 115200;
}

LocoNetEsp32Port locoNetPort(kLocoNetRxPin, kLocoNetTxPin);
SerialMessageLog messageLog;
LocoNetMessageLogger logger(locoNetPort, messageLog);

void setup()
{
    Serial.begin(kSerialBaudRate);
    locoNetPort.begin();
}

void loop()
{
    locoNetPort.update();
    logger.update();
}
```

- [ ] **Step 2: Build and flash**

Run: `pio run -e esp32dev --target upload`
Expected: `SUCCESS`, board resets and starts running.

- [ ] **Step 3: Smoke-test against a live LocoNet bus**

Run: `pio device monitor` (115200 baud) with the adapter's `J1`/RJ12 wired into an open DR5000 port (or a second LocoNet device) per `docs/breadboard-build-guide.md`. Expected: hex-formatted message lines appear as soon as any LocoNet traffic occurs (heartbeats, throttle activity) — confirms `InverseLogic = true` and the RX wiring are correct.

- [ ] **Step 4: Write `README.md`**

```markdown
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
```

- [ ] **Step 5: Write `CLAUDE.md`**

```markdown
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
  application objects together, calls `begin()` once, and calls
  non-blocking `update()` methods from `loop()`. No business logic lives
  here. File-scope object construction in `main.cpp` is the one accepted
  exception to "no globals" — Arduino's `setup()`/`loop()` model has no
  other place to hold constructed objects across calls.
- `LocoNetEsp32Port` (`lib/Loco2MqttCore/src/adapters/LocoNetEsp32Port.cpp`)
  bridges the vendor library's plain-C-callback receive API into a
  translation-unit-local queue — a deliberate, narrowly-scoped exception to
  "no statics," matching the existing pattern in
  `MaltbeeController`'s `MrrwaLocoNetFeedbackSource.cpp`. Only one instance
  may exist at a time.
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
```

- [ ] **Step 6: Final full verification**

Run: `pio test -e native` — expect every suite from Tasks 1–6 still passing.
Run: `pio run -e esp32dev` — expect `SUCCESS`.

- [ ] **Step 7: Commit**

```bash
git add src/main.cpp README.md CLAUDE.md
git commit -m "feat: wire LocoNet message logging vertical slice in the composition root"
```

---

## Out of scope

- MQTT bridging (the project's eventual purpose) — no `PubSubClient`/broker
  code, topics, or JMRI integration in this plan.
- `LocoNetPort::send()` being driven by any real application logic — it is
  implemented and build-checked (Task 7) because the port contract requires
  it, but nothing calls it yet.
- The PCB phase, KiCad design, or any BOM/ordering concerns from the build
  guide's "Notes for the PCB phase" section.
- Any consumer of `DigitalPin`/`EspDigitalPin` beyond the Task 3 build-check
  (e.g. a status LED) — scaffolded per spec, not yet needed by any real
  feature.
