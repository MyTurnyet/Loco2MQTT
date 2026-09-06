# NodeConfig & Commissioning Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Let Loco2MQTT's WiFi credentials be configured at commission time — via a bench-serial console or a BOOT-button-triggered captive-portal AP — instead of hardcoded, so the future MQTT bridge sub-project never needs to touch source code to join a layout's WiFi.

**Architecture:** Hexagonal, matching the existing codebase exactly: new domain value objects (`LocoNetAdapterConfig`, `ParsedCommand`, `BootMode`) and pure functions (`CommandLineParser`, `BootModeSelector`, `SetupFormRenderer`) are Arduino-free and native-testable; new ports (`ConfigStore`, `UartPort`, `DigitalInput`, `Clock`, `SetupModeRequestStore`, `RebootTrigger`) are pure interfaces with hand-written fakes; new application services (`CommissioningSession`, `ButtonSetupModeTrigger`) depend only on ports; adapters (`SerialCommissioningAdapter`, `WebFormCommissioningAdapter`, five ESP32 hardware shims, `CaptivePortalServer`) implement those ports, with every hardware-touching one guarded by `#ifdef ARDUINO`. `src/main.cpp` reads a `BootMode` once at startup and branches: `WirelessSetup` never constructs the existing LocoNet-logging vertical slice at all; `Normal`/`NeedsCommissioning` run it exactly as today.

**Tech Stack:** C++17, PlatformIO (`native` env for Catch2 unit tests, `esp32dev` env for the real firmware), Arduino core libraries already available on `esp32dev` (`Preferences`, `WiFi`, `DNSServer`, `WebServer`) — no new `lib_deps` entries required.

**Note on one port not named in the spec:** the design doc (`docs/superpowers/specs/2026-09-05-node-config-commissioning-design.md`) says `WebFormCommissioningAdapter` "triggers a reboot back into normal boot mode" and lists "valid submission saves and reboots" as a native test case (Section 7). To make that native-testable without pulling in `ESP.restart()`, this plan introduces one small port not explicitly named in the spec text: `RebootTrigger` (`reboot()`), implemented for real by `EspRebootTrigger` (`ESP.restart()`) and faked by `FakeRebootTrigger` (a call counter). This follows the same pattern as every other hardware capability in this codebase (`DigitalPin`, `LocoNetPort`, `MessageLog`) and does not change any behavior the spec describes.

## Global Constraints

- Config domain object holds exactly two fields — `wifiSsid`, `wifiPassword` — no broker address/port/credentials fields (spec Section 2: the bridge will run its own on-device broker, not connect to an external one).
- `isComplete()` is true only when both fields are non-empty (spec Section 2).
- Bench-serial line length cap: `kMaxLineLength = 128` bytes; longer lines are rejected as an unknown command, not buffered without bound (spec Section 3).
- BOOT-button hold duration to trigger wireless setup: `kHoldDurationMs = 3000` (spec Section 4). GPIO0, active-low, internal pull-up (spec Section 4 / existing ESP32 BOOT-button wiring).
- Captive-portal AP: fixed name `"Loco2MQTT-Setup"`, no passphrase (open) (spec Section 5).
- **Security rule, non-negotiable:** the stored WiFi password must never be reflected back into the rendered web form — the password field always renders empty (spec Section 5).
- `BootMode` has exactly three values: `Normal`, `NeedsCommissioning`, `WirelessSetup` (spec Section 6). `WirelessSetup` wins whenever a setup request is pending, regardless of config completeness.
- In `WirelessSetup` mode, the existing LocoNet-logging vertical slice (`LocoNetEsp32Port`/`SerialMessageLog`/`LocoNetMessageLogger`) must not be constructed at all (spec Section 6).
- `SerialCommissioningAdapter` (bench-serial console) only runs in `NeedsCommissioning`; `ButtonSetupModeTrigger` runs in both `Normal` and `NeedsCommissioning`, never in `WirelessSetup` (spec Section 6).
- No new PlatformIO `lib_deps` — `Preferences`, `WiFi`, `DNSServer`, `WebServer` are already part of the `esp32dev` environment's Arduino core (spec Section 6).
- Every class except the five genuine hardware shims (`NvsConfigStore`, `EspUartPort`, `EspDigitalInput`, `ArduinoClock`, `NvsSetupModeRequestStore`) plus `EspRebootTrigger` and `CaptivePortalServer` must be native-testable and covered by a Catch2 suite; those seven get build-check-only verification via `pio run -e esp32dev` (spec Section 7, matching this project's existing convention for `LocoNetEsp32Port`/`SerialMessageLog`/`EspDigitalPin`).
- **Discovered during Task 2, binding on every remaining task:** in this project's `native` test environment, a test file that only *transitively* reaches a domain class with out-of-line (`.cpp`) methods — through a fake or another header, never naming that class's own header directly — fails to link (`undefined reference`), confirmed with a clean `.pio/build/native` rebuild. Every test file must directly `#include` the header of any class it names/constructs/compares whose methods live in a `.cpp` (`LocoNetAdapterConfig`, `ParsedCommand`), even when a fake, port, or another domain header it already includes would otherwise make the type resolve. Classes that are fully header-only (`Level`, `LocoNetMessage`, every port, every fake) are unaffected — this is a linker-visibility issue, not a compile-visibility one.
- Project-wide (CLAUDE.md, unchanged by this plan but binding on every task): methods ≤ 8 lines, cognitive complexity < 4; TDD with no mocking framework, hand-written fakes only, in `test/support/`; immutable domain value objects with `with...()` copy-returning mutators; constructor-injected dependencies everywhere; hardware adapters guarded with `#ifdef ARDUINO`; Allman brace style, 4-space indent, `#pragma once` header guards (match existing files exactly).

---

### Task 1: `LocoNetAdapterConfig` domain object

**Files:**
- Create: `lib/Loco2MqttCore/src/domain/LocoNetAdapterConfig.h`
- Create: `lib/Loco2MqttCore/src/domain/LocoNetAdapterConfig.cpp`
- Test: `test/test_loco_net_adapter_config/test_main.cpp`

**Interfaces:**
- Produces: `class LocoNetAdapterConfig` with `LocoNetAdapterConfig()` (both fields empty), `LocoNetAdapterConfig(std::string wifiSsid, std::string wifiPassword)`, `const std::string& wifiSsid() const`, `const std::string& wifiPassword() const`, `bool isComplete() const`, `LocoNetAdapterConfig withWifiSsid(const std::string&) const`, `LocoNetAdapterConfig withWifiPassword(const std::string&) const`, `operator==`, `operator!=`.

- [ ] **Step 1: Write the failing test**

Create `test/test_loco_net_adapter_config/test_main.cpp`:

```cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/LocoNetAdapterConfig.h"

TEST_CASE("A default-constructed config is not complete")
{
    LocoNetAdapterConfig config;

    REQUIRE(config.isComplete() == false);
}

TEST_CASE("A config with only an SSID is not complete")
{
    LocoNetAdapterConfig config("MyHomeWifi", "");

    REQUIRE(config.isComplete() == false);
}

TEST_CASE("A config with only a password is not complete")
{
    LocoNetAdapterConfig config("", "hunter2");

    REQUIRE(config.isComplete() == false);
}

TEST_CASE("A config with both fields is complete")
{
    LocoNetAdapterConfig config("MyHomeWifi", "hunter2");

    REQUIRE(config.isComplete() == true);
}

TEST_CASE("withWifiSsid returns a new config with the SSID changed and password unchanged")
{
    LocoNetAdapterConfig config("Old", "hunter2");

    LocoNetAdapterConfig updated = config.withWifiSsid("New");

    REQUIRE(updated.wifiSsid() == "New");
    REQUIRE(updated.wifiPassword() == "hunter2");
}

TEST_CASE("withWifiPassword returns a new config with the password changed and SSID unchanged")
{
    LocoNetAdapterConfig config("MyHomeWifi", "old-pass");

    LocoNetAdapterConfig updated = config.withWifiPassword("new-pass");

    REQUIRE(updated.wifiSsid() == "MyHomeWifi");
    REQUIRE(updated.wifiPassword() == "new-pass");
}

TEST_CASE("Configs with equal fields compare equal")
{
    LocoNetAdapterConfig a("MyHomeWifi", "hunter2");
    LocoNetAdapterConfig b("MyHomeWifi", "hunter2");

    REQUIRE(a == b);
}

TEST_CASE("Configs with different fields compare unequal")
{
    LocoNetAdapterConfig a("MyHomeWifi", "hunter2");
    LocoNetAdapterConfig b("MyHomeWifi", "different");

    REQUIRE(a != b);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `pio test -e native -f test_loco_net_adapter_config`
Expected: FAIL to compile — `domain/LocoNetAdapterConfig.h` does not exist yet.

- [ ] **Step 3: Write minimal implementation**

Create `lib/Loco2MqttCore/src/domain/LocoNetAdapterConfig.h`:

```cpp
#pragma once

#include <string>

class LocoNetAdapterConfig
{
public:
    LocoNetAdapterConfig() = default;
    LocoNetAdapterConfig(std::string wifiSsid, std::string wifiPassword);

    const std::string& wifiSsid() const;
    const std::string& wifiPassword() const;
    bool isComplete() const;

    LocoNetAdapterConfig withWifiSsid(const std::string& value) const;
    LocoNetAdapterConfig withWifiPassword(const std::string& value) const;

    bool operator==(const LocoNetAdapterConfig& other) const;
    bool operator!=(const LocoNetAdapterConfig& other) const;

private:
    std::string wifiSsid_;
    std::string wifiPassword_;
};
```

Create `lib/Loco2MqttCore/src/domain/LocoNetAdapterConfig.cpp`:

```cpp
#include "LocoNetAdapterConfig.h"

LocoNetAdapterConfig::LocoNetAdapterConfig(std::string wifiSsid, std::string wifiPassword)
    : wifiSsid_(std::move(wifiSsid)), wifiPassword_(std::move(wifiPassword))
{
}

const std::string& LocoNetAdapterConfig::wifiSsid() const
{
    return wifiSsid_;
}

const std::string& LocoNetAdapterConfig::wifiPassword() const
{
    return wifiPassword_;
}

bool LocoNetAdapterConfig::isComplete() const
{
    return !wifiSsid_.empty() && !wifiPassword_.empty();
}

LocoNetAdapterConfig LocoNetAdapterConfig::withWifiSsid(const std::string& value) const
{
    return LocoNetAdapterConfig(value, wifiPassword_);
}

LocoNetAdapterConfig LocoNetAdapterConfig::withWifiPassword(const std::string& value) const
{
    return LocoNetAdapterConfig(wifiSsid_, value);
}

bool LocoNetAdapterConfig::operator==(const LocoNetAdapterConfig& other) const
{
    return wifiSsid_ == other.wifiSsid_ && wifiPassword_ == other.wifiPassword_;
}

bool LocoNetAdapterConfig::operator!=(const LocoNetAdapterConfig& other) const
{
    return !(*this == other);
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `pio test -e native -f test_loco_net_adapter_config`
Expected: PASS, 8/8 assertions.

- [ ] **Step 5: Commit**

```bash
git add lib/Loco2MqttCore/src/domain/LocoNetAdapterConfig.h lib/Loco2MqttCore/src/domain/LocoNetAdapterConfig.cpp test/test_loco_net_adapter_config/test_main.cpp
git commit -m "feat: add LocoNetAdapterConfig domain object"
```

---

### Task 2: `ConfigStore` port + `FakeConfigStore`

**Files:**
- Create: `lib/Loco2MqttCore/src/ports/ConfigStore.h`
- Create: `test/support/FakeConfigStore.h`
- Test: `test/test_fake_config_store/test_main.cpp`

**Interfaces:**
- Consumes: `LocoNetAdapterConfig` (Task 1) — default constructor, equality.
- Produces: `class ConfigStore` with `virtual LocoNetAdapterConfig load() = 0;` and `virtual void save(const LocoNetAdapterConfig&) = 0;`. `class FakeConfigStore : public ConfigStore` with an additional `int saveCount() const` for tests to assert how many times `save()` was called.

- [ ] **Step 1: Write the failing test**

Create `test/test_fake_config_store/test_main.cpp`:

```cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "support/FakeConfigStore.h"

TEST_CASE("load() returns a default-constructed config when nothing has been saved")
{
    FakeConfigStore store;

    LocoNetAdapterConfig config = store.load();

    REQUIRE(config == LocoNetAdapterConfig());
}

TEST_CASE("save() then load() returns the saved config")
{
    FakeConfigStore store;
    LocoNetAdapterConfig config("MyHomeWifi", "hunter2");

    store.save(config);

    REQUIRE(store.load() == config);
}

TEST_CASE("saveCount() tracks how many times save() was called")
{
    FakeConfigStore store;

    store.save(LocoNetAdapterConfig("A", "1"));
    store.save(LocoNetAdapterConfig("B", "2"));

    REQUIRE(store.saveCount() == 2);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `pio test -e native -f test_fake_config_store`
Expected: FAIL to compile — `ports/ConfigStore.h` and `support/FakeConfigStore.h` do not exist yet.

- [ ] **Step 3: Write minimal implementation**

Create `lib/Loco2MqttCore/src/ports/ConfigStore.h`:

```cpp
#pragma once

#include "domain/LocoNetAdapterConfig.h"

class ConfigStore
{
public:
    virtual ~ConfigStore() = default;
    virtual LocoNetAdapterConfig load() = 0;
    virtual void save(const LocoNetAdapterConfig& config) = 0;
};
```

Create `test/support/FakeConfigStore.h`:

```cpp
#pragma once

#include "ports/ConfigStore.h"

class FakeConfigStore : public ConfigStore
{
public:
    LocoNetAdapterConfig load() override
    {
        return saved_;
    }

    void save(const LocoNetAdapterConfig& config) override
    {
        saved_ = config;
        saveCount_++;
    }

    int saveCount() const
    {
        return saveCount_;
    }

private:
    LocoNetAdapterConfig saved_;
    int saveCount_ = 0;
};
```

- [ ] **Step 4: Run test to verify it passes**

Run: `pio test -e native -f test_fake_config_store`
Expected: PASS, 3/3 assertions.

- [ ] **Step 5: Commit**

```bash
git add lib/Loco2MqttCore/src/ports/ConfigStore.h test/support/FakeConfigStore.h test/test_fake_config_store/test_main.cpp
git commit -m "feat: add ConfigStore port and FakeConfigStore"
```

---

### Task 3: `ParsedCommand` + `CommandLineParser` domain objects

**Files:**
- Create: `lib/Loco2MqttCore/src/domain/ParsedCommand.h`
- Create: `lib/Loco2MqttCore/src/domain/ParsedCommand.cpp`
- Create: `lib/Loco2MqttCore/src/domain/CommandLineParser.h`
- Create: `lib/Loco2MqttCore/src/domain/CommandLineParser.cpp`
- Test: `test/test_command_line_parser/test_main.cpp`

**Interfaces:**
- Produces: `enum class CommandType { SetSsid, SetPassword, Show, Save, Unknown };` and `class ParsedCommand` with static factories `setSsid(const std::string&)`, `setPassword(const std::string&)`, `show()`, `save()`, `unknown(const std::string& rawLine)`, plus `CommandType type() const` and `const std::string& value() const`. Free function `ParsedCommand parseCommandLine(const std::string& line);`.
- Command syntax: `set-ssid <value>`, `set-password <value>`, `show`, `save` (exact verbs, one space separator, no trailing arguments on `show`/`save`). Anything else — empty line, unknown verb, missing required argument, or an unexpected trailing argument on `show`/`save` — parses to `Unknown` with `value()` equal to the original raw line.

- [ ] **Step 1: Write the failing test**

Create `test/test_command_line_parser/test_main.cpp`:

```cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/CommandLineParser.h"
#include "domain/ParsedCommand.h"

TEST_CASE("parses set-ssid with its value")
{
    ParsedCommand command = parseCommandLine("set-ssid MyHomeWifi");

    REQUIRE(command.type() == CommandType::SetSsid);
    REQUIRE(command.value() == "MyHomeWifi");
}

TEST_CASE("parses set-password with its value")
{
    ParsedCommand command = parseCommandLine("set-password hunter2");

    REQUIRE(command.type() == CommandType::SetPassword);
    REQUIRE(command.value() == "hunter2");
}

TEST_CASE("parses show with no argument")
{
    ParsedCommand command = parseCommandLine("show");

    REQUIRE(command.type() == CommandType::Show);
}

TEST_CASE("parses save with no argument")
{
    ParsedCommand command = parseCommandLine("save");

    REQUIRE(command.type() == CommandType::Save);
}

TEST_CASE("an empty line is unknown")
{
    ParsedCommand command = parseCommandLine("");

    REQUIRE(command.type() == CommandType::Unknown);
    REQUIRE(command.value() == "");
}

TEST_CASE("an unrecognized verb is unknown")
{
    ParsedCommand command = parseCommandLine("bogus");

    REQUIRE(command.type() == CommandType::Unknown);
    REQUIRE(command.value() == "bogus");
}

TEST_CASE("set-ssid with no argument is unknown")
{
    ParsedCommand command = parseCommandLine("set-ssid");

    REQUIRE(command.type() == CommandType::Unknown);
    REQUIRE(command.value() == "set-ssid");
}

TEST_CASE("show with an unexpected argument is unknown")
{
    ParsedCommand command = parseCommandLine("show extra");

    REQUIRE(command.type() == CommandType::Unknown);
    REQUIRE(command.value() == "show extra");
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `pio test -e native -f test_command_line_parser`
Expected: FAIL to compile — `domain/CommandLineParser.h` does not exist yet.

- [ ] **Step 3: Write minimal implementation**

Create `lib/Loco2MqttCore/src/domain/ParsedCommand.h`:

```cpp
#pragma once

#include <string>

enum class CommandType
{
    SetSsid,
    SetPassword,
    Show,
    Save,
    Unknown
};

class ParsedCommand
{
public:
    static ParsedCommand setSsid(const std::string& value);
    static ParsedCommand setPassword(const std::string& value);
    static ParsedCommand show();
    static ParsedCommand save();
    static ParsedCommand unknown(const std::string& rawLine);

    CommandType type() const;
    const std::string& value() const;

private:
    ParsedCommand(CommandType type, std::string value);

    CommandType type_;
    std::string value_;
};
```

Create `lib/Loco2MqttCore/src/domain/ParsedCommand.cpp`:

```cpp
#include "ParsedCommand.h"

ParsedCommand::ParsedCommand(CommandType type, std::string value)
    : type_(type), value_(std::move(value))
{
}

ParsedCommand ParsedCommand::setSsid(const std::string& value)
{
    return ParsedCommand(CommandType::SetSsid, value);
}

ParsedCommand ParsedCommand::setPassword(const std::string& value)
{
    return ParsedCommand(CommandType::SetPassword, value);
}

ParsedCommand ParsedCommand::show()
{
    return ParsedCommand(CommandType::Show, "");
}

ParsedCommand ParsedCommand::save()
{
    return ParsedCommand(CommandType::Save, "");
}

ParsedCommand ParsedCommand::unknown(const std::string& rawLine)
{
    return ParsedCommand(CommandType::Unknown, rawLine);
}

CommandType ParsedCommand::type() const
{
    return type_;
}

const std::string& ParsedCommand::value() const
{
    return value_;
}
```

Create `lib/Loco2MqttCore/src/domain/CommandLineParser.h`:

```cpp
#pragma once

#include <string>

#include "ParsedCommand.h"

ParsedCommand parseCommandLine(const std::string& line);
```

Create `lib/Loco2MqttCore/src/domain/CommandLineParser.cpp`:

```cpp
#include "CommandLineParser.h"

namespace
{
    std::string verbOf(const std::string& line)
    {
        return line.substr(0, line.find(' '));
    }

    std::string argumentOf(const std::string& line)
    {
        const auto spacePos = line.find(' ');
        return spacePos == std::string::npos ? "" : line.substr(spacePos + 1);
    }
}

ParsedCommand parseCommandLine(const std::string& line)
{
    const std::string verb = verbOf(line);
    const std::string argument = argumentOf(line);

    if (verb == "set-ssid" && !argument.empty())
    {
        return ParsedCommand::setSsid(argument);
    }
    if (verb == "set-password" && !argument.empty())
    {
        return ParsedCommand::setPassword(argument);
    }
    if (verb == "show" && argument.empty())
    {
        return ParsedCommand::show();
    }
    if (verb == "save" && argument.empty())
    {
        return ParsedCommand::save();
    }
    return ParsedCommand::unknown(line);
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `pio test -e native -f test_command_line_parser`
Expected: PASS, 8/8 assertions.

- [ ] **Step 5: Commit**

```bash
git add lib/Loco2MqttCore/src/domain/ParsedCommand.h lib/Loco2MqttCore/src/domain/ParsedCommand.cpp lib/Loco2MqttCore/src/domain/CommandLineParser.h lib/Loco2MqttCore/src/domain/CommandLineParser.cpp test/test_command_line_parser/test_main.cpp
git commit -m "feat: add ParsedCommand and CommandLineParser"
```

---

### Task 4: `CommissioningSession` application service

**Files:**
- Create: `lib/Loco2MqttCore/src/application/CommissioningSession.h`
- Create: `lib/Loco2MqttCore/src/application/CommissioningSession.cpp`
- Test: `test/test_commissioning_session/test_main.cpp`

**Interfaces:**
- Consumes: `ConfigStore` (Task 2) — `load()`/`save()`; `FakeConfigStore` (Task 2) — `saveCount()`; `ParsedCommand`/`CommandType` (Task 3); `LocoNetAdapterConfig` (Task 1) — `withWifiSsid()`, `withWifiPassword()`, `wifiSsid()`, `wifiPassword()`.
- Produces: `class CommissioningSession` with `explicit CommissioningSession(ConfigStore& configStore)` and `std::string apply(const ParsedCommand& command)`. Response strings: `"OK"` for `SetSsid`/`SetPassword`, `"ssid=<ssid> password=<set|unset>"` for `Show` (never the literal password), `"SAVED"` for `Save` (which is the only command that calls `ConfigStore::save()`), `"ERR unknown command: <rawLine>"` for `Unknown`.

- [ ] **Step 1: Write the failing test**

Create `test/test_commissioning_session/test_main.cpp`:

```cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "application/CommissioningSession.h"
#include "domain/CommandLineParser.h"
#include "domain/LocoNetAdapterConfig.h"
#include "domain/ParsedCommand.h"
#include "support/FakeConfigStore.h"

TEST_CASE("set-ssid returns OK and is reflected by a later show")
{
    FakeConfigStore store;
    CommissioningSession session(store);

    std::string reply = session.apply(parseCommandLine("set-ssid MyHomeWifi"));

    REQUIRE(reply == "OK");
    REQUIRE(session.apply(parseCommandLine("show")) == "ssid=MyHomeWifi password=unset");
}

TEST_CASE("set-password returns OK and show never reveals the password value")
{
    FakeConfigStore store;
    CommissioningSession session(store);

    std::string reply = session.apply(parseCommandLine("set-password hunter2"));

    REQUIRE(reply == "OK");
    std::string shown = session.apply(parseCommandLine("show"));
    REQUIRE(shown == "ssid= password=set");
    REQUIRE(shown.find("hunter2") == std::string::npos);
}

TEST_CASE("show with nothing set reports an empty ssid and an unset password")
{
    FakeConfigStore store;
    CommissioningSession session(store);

    REQUIRE(session.apply(parseCommandLine("show")) == "ssid= password=unset");
}

TEST_CASE("setting fields does not save until an explicit save command")
{
    FakeConfigStore store;
    CommissioningSession session(store);

    session.apply(parseCommandLine("set-ssid MyHomeWifi"));
    session.apply(parseCommandLine("set-password hunter2"));

    REQUIRE(store.saveCount() == 0);
}

TEST_CASE("save persists the pending config and returns SAVED")
{
    FakeConfigStore store;
    CommissioningSession session(store);
    session.apply(parseCommandLine("set-ssid MyHomeWifi"));
    session.apply(parseCommandLine("set-password hunter2"));

    std::string reply = session.apply(parseCommandLine("save"));

    REQUIRE(reply == "SAVED");
    REQUIRE(store.saveCount() == 1);
    REQUIRE(store.load() == LocoNetAdapterConfig("MyHomeWifi", "hunter2"));
}

TEST_CASE("an unknown command reports the raw line and does not save")
{
    FakeConfigStore store;
    CommissioningSession session(store);

    std::string reply = session.apply(parseCommandLine("garbage"));

    REQUIRE(reply == "ERR unknown command: garbage");
    REQUIRE(store.saveCount() == 0);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `pio test -e native -f test_commissioning_session`
Expected: FAIL to compile — `application/CommissioningSession.h` does not exist yet.

- [ ] **Step 3: Write minimal implementation**

Create `lib/Loco2MqttCore/src/application/CommissioningSession.h`:

```cpp
#pragma once

#include <string>

#include "domain/LocoNetAdapterConfig.h"
#include "domain/ParsedCommand.h"
#include "ports/ConfigStore.h"

class CommissioningSession
{
public:
    explicit CommissioningSession(ConfigStore& configStore);

    std::string apply(const ParsedCommand& command);

private:
    std::string applySetSsid(const std::string& value);
    std::string applySetPassword(const std::string& value);
    std::string applyShow() const;
    std::string applySave();

    ConfigStore& configStore_;
    LocoNetAdapterConfig pending_;
};
```

Create `lib/Loco2MqttCore/src/application/CommissioningSession.cpp`:

```cpp
#include "CommissioningSession.h"

CommissioningSession::CommissioningSession(ConfigStore& configStore)
    : configStore_(configStore)
{
}

std::string CommissioningSession::apply(const ParsedCommand& command)
{
    switch (command.type())
    {
        case CommandType::SetSsid:
            return applySetSsid(command.value());
        case CommandType::SetPassword:
            return applySetPassword(command.value());
        case CommandType::Show:
            return applyShow();
        case CommandType::Save:
            return applySave();
        default:
            return "ERR unknown command: " + command.value();
    }
}

std::string CommissioningSession::applySetSsid(const std::string& value)
{
    pending_ = pending_.withWifiSsid(value);
    return "OK";
}

std::string CommissioningSession::applySetPassword(const std::string& value)
{
    pending_ = pending_.withWifiPassword(value);
    return "OK";
}

std::string CommissioningSession::applyShow() const
{
    const std::string passwordState = pending_.wifiPassword().empty() ? "unset" : "set";
    return "ssid=" + pending_.wifiSsid() + " password=" + passwordState;
}

std::string CommissioningSession::applySave()
{
    configStore_.save(pending_);
    return "SAVED";
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `pio test -e native -f test_commissioning_session`
Expected: PASS, 6/6 test cases (11 assertions).

- [ ] **Step 5: Commit**

```bash
git add lib/Loco2MqttCore/src/application/CommissioningSession.h lib/Loco2MqttCore/src/application/CommissioningSession.cpp test/test_commissioning_session/test_main.cpp
git commit -m "feat: add CommissioningSession application service"
```

---

### Task 5: `UartPort` port + `FakeUartPort`

**Files:**
- Create: `lib/Loco2MqttCore/src/ports/UartPort.h`
- Create: `test/support/FakeUartPort.h`
- Test: `test/test_fake_uart_port/test_main.cpp`

**Interfaces:**
- Produces: `class UartPort` with `virtual std::optional<std::string> readLine() = 0;` and `virtual void writeLine(const std::string&) = 0;`. `class FakeUartPort : public UartPort` with `void enqueueLine(const std::string&)` (feeds input) and `const std::vector<std::string>& writtenLines() const` (captures output).

- [ ] **Step 1: Write the failing test**

Create `test/test_fake_uart_port/test_main.cpp`:

```cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "support/FakeUartPort.h"

TEST_CASE("readLine returns nullopt when nothing is queued")
{
    FakeUartPort uart;

    REQUIRE(uart.readLine() == std::nullopt);
}

TEST_CASE("readLine returns an enqueued line, then nullopt again")
{
    FakeUartPort uart;
    uart.enqueueLine("hello");

    REQUIRE(uart.readLine() == std::optional<std::string>("hello"));
    REQUIRE(uart.readLine() == std::nullopt);
}

TEST_CASE("enqueued lines are read back in FIFO order")
{
    FakeUartPort uart;
    uart.enqueueLine("first");
    uart.enqueueLine("second");

    REQUIRE(uart.readLine() == std::optional<std::string>("first"));
    REQUIRE(uart.readLine() == std::optional<std::string>("second"));
}

TEST_CASE("writeLine records written lines in order")
{
    FakeUartPort uart;

    uart.writeLine("OK");
    uart.writeLine("SAVED");

    REQUIRE(uart.writtenLines() == std::vector<std::string>{"OK", "SAVED"});
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `pio test -e native -f test_fake_uart_port`
Expected: FAIL to compile — `ports/UartPort.h` and `support/FakeUartPort.h` do not exist yet.

- [ ] **Step 3: Write minimal implementation**

Create `lib/Loco2MqttCore/src/ports/UartPort.h`:

```cpp
#pragma once

#include <optional>
#include <string>

class UartPort
{
public:
    virtual ~UartPort() = default;
    virtual std::optional<std::string> readLine() = 0;
    virtual void writeLine(const std::string& line) = 0;
};
```

Create `test/support/FakeUartPort.h`:

```cpp
#pragma once

#include <queue>
#include <string>
#include <vector>

#include "ports/UartPort.h"

class FakeUartPort : public UartPort
{
public:
    void enqueueLine(const std::string& line)
    {
        inbox_.push(line);
    }

    std::optional<std::string> readLine() override
    {
        if (inbox_.empty())
        {
            return std::nullopt;
        }
        const std::string line = inbox_.front();
        inbox_.pop();
        return line;
    }

    void writeLine(const std::string& line) override
    {
        outbox_.push_back(line);
    }

    const std::vector<std::string>& writtenLines() const
    {
        return outbox_;
    }

private:
    std::queue<std::string> inbox_;
    std::vector<std::string> outbox_;
};
```

- [ ] **Step 4: Run test to verify it passes**

Run: `pio test -e native -f test_fake_uart_port`
Expected: PASS, 4/4 test cases.

- [ ] **Step 5: Commit**

```bash
git add lib/Loco2MqttCore/src/ports/UartPort.h test/support/FakeUartPort.h test/test_fake_uart_port/test_main.cpp
git commit -m "feat: add UartPort port and FakeUartPort"
```

---

### Task 6: `SerialCommissioningAdapter`

**Files:**
- Create: `lib/Loco2MqttCore/src/adapters/SerialCommissioningAdapter.h`
- Create: `lib/Loco2MqttCore/src/adapters/SerialCommissioningAdapter.cpp`
- Test: `test/test_serial_commissioning_adapter/test_main.cpp`

**Interfaces:**
- Consumes: `UartPort`/`FakeUartPort` (Task 5); `CommissioningSession` (Task 4); `CommandLineParser`/`ParsedCommand` (Task 3); `FakeConfigStore` (Task 2, used to construct a `CommissioningSession` in tests).
- Produces: `class SerialCommissioningAdapter` with `SerialCommissioningAdapter(UartPort& uart, CommissioningSession& session)` and `void update()`. `kMaxLineLength = 128` (see Global Constraints) — lines longer than this are treated as `Unknown` before parsing.

- [ ] **Step 1: Write the failing test**

Create `test/test_serial_commissioning_adapter/test_main.cpp`:

```cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include <string>

#include "adapters/SerialCommissioningAdapter.h"
#include "application/CommissioningSession.h"
#include "domain/LocoNetAdapterConfig.h"
#include "support/FakeConfigStore.h"
#include "support/FakeUartPort.h"

TEST_CASE("update does nothing when no line is available")
{
    FakeUartPort uart;
    FakeConfigStore store;
    CommissioningSession session(store);
    SerialCommissioningAdapter adapter(uart, session);

    adapter.update();

    REQUIRE(uart.writtenLines().empty());
}

TEST_CASE("update parses a queued line and writes the session's reply")
{
    FakeUartPort uart;
    FakeConfigStore store;
    CommissioningSession session(store);
    SerialCommissioningAdapter adapter(uart, session);
    uart.enqueueLine("set-ssid MyHomeWifi");

    adapter.update();

    REQUIRE(uart.writtenLines() == std::vector<std::string>{"OK"});
}

TEST_CASE("update supports a full set-and-save round trip")
{
    FakeUartPort uart;
    FakeConfigStore store;
    CommissioningSession session(store);
    SerialCommissioningAdapter adapter(uart, session);
    uart.enqueueLine("set-ssid MyHomeWifi");
    uart.enqueueLine("set-password hunter2");
    uart.enqueueLine("save");

    adapter.update();
    adapter.update();
    adapter.update();

    REQUIRE(uart.writtenLines() == std::vector<std::string>{"OK", "OK", "SAVED"});
    REQUIRE(store.load() == LocoNetAdapterConfig("MyHomeWifi", "hunter2"));
}

TEST_CASE("update rejects a line longer than kMaxLineLength as unknown")
{
    FakeUartPort uart;
    FakeConfigStore store;
    CommissioningSession session(store);
    SerialCommissioningAdapter adapter(uart, session);
    const std::string overlong(129, 'a');
    uart.enqueueLine(overlong);

    adapter.update();

    REQUIRE(uart.writtenLines() == std::vector<std::string>{"ERR unknown command: " + overlong});
}

TEST_CASE("update reports an unrecognized command")
{
    FakeUartPort uart;
    FakeConfigStore store;
    CommissioningSession session(store);
    SerialCommissioningAdapter adapter(uart, session);
    uart.enqueueLine("bogus");

    adapter.update();

    REQUIRE(uart.writtenLines() == std::vector<std::string>{"ERR unknown command: bogus"});
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `pio test -e native -f test_serial_commissioning_adapter`
Expected: FAIL to compile — `adapters/SerialCommissioningAdapter.h` does not exist yet.

- [ ] **Step 3: Write minimal implementation**

Create `lib/Loco2MqttCore/src/adapters/SerialCommissioningAdapter.h`:

```cpp
#pragma once

#include <cstddef>
#include <string>

#include "application/CommissioningSession.h"
#include "domain/ParsedCommand.h"
#include "ports/UartPort.h"

class SerialCommissioningAdapter
{
public:
    SerialCommissioningAdapter(UartPort& uart, CommissioningSession& session);

    void update();

private:
    ParsedCommand parseLine(const std::string& line) const;

    UartPort& uart_;
    CommissioningSession& session_;

    static constexpr std::size_t kMaxLineLength = 128;
};
```

Create `lib/Loco2MqttCore/src/adapters/SerialCommissioningAdapter.cpp`:

```cpp
#include "SerialCommissioningAdapter.h"

#include "domain/CommandLineParser.h"

SerialCommissioningAdapter::SerialCommissioningAdapter(UartPort& uart, CommissioningSession& session)
    : uart_(uart), session_(session)
{
}

void SerialCommissioningAdapter::update()
{
    const std::optional<std::string> line = uart_.readLine();
    if (!line.has_value())
    {
        return;
    }
    const ParsedCommand command = parseLine(*line);
    uart_.writeLine(session_.apply(command));
}

ParsedCommand SerialCommissioningAdapter::parseLine(const std::string& line) const
{
    if (line.size() > kMaxLineLength)
    {
        return ParsedCommand::unknown(line);
    }
    return parseCommandLine(line);
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `pio test -e native -f test_serial_commissioning_adapter`
Expected: PASS, 5/5 test cases.

- [ ] **Step 5: Commit**

```bash
git add lib/Loco2MqttCore/src/adapters/SerialCommissioningAdapter.h lib/Loco2MqttCore/src/adapters/SerialCommissioningAdapter.cpp test/test_serial_commissioning_adapter/test_main.cpp
git commit -m "feat: add SerialCommissioningAdapter"
```

---

### Task 7: `DigitalInput` port + `FakeDigitalInput`

**Files:**
- Create: `lib/Loco2MqttCore/src/ports/DigitalInput.h`
- Create: `test/support/FakeDigitalInput.h`
- Test: `test/test_fake_digital_input/test_main.cpp`

**Interfaces:**
- Produces: `class DigitalInput` with `virtual bool isActive() const = 0;`. `class FakeDigitalInput : public DigitalInput` with `void setActive(bool)`. This is a new, read-only port — distinct from the existing write-only `DigitalPin` (`ports/DigitalPin.h`), which this task does not modify.

- [ ] **Step 1: Write the failing test**

Create `test/test_fake_digital_input/test_main.cpp`:

```cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "support/FakeDigitalInput.h"

TEST_CASE("A fake digital input begins inactive")
{
    FakeDigitalInput input;

    REQUIRE(input.isActive() == false);
}

TEST_CASE("setActive(true) makes isActive() report true")
{
    FakeDigitalInput input;

    input.setActive(true);

    REQUIRE(input.isActive() == true);
}

TEST_CASE("setActive(false) makes isActive() report false again")
{
    FakeDigitalInput input;
    input.setActive(true);

    input.setActive(false);

    REQUIRE(input.isActive() == false);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `pio test -e native -f test_fake_digital_input`
Expected: FAIL to compile — `ports/DigitalInput.h` and `support/FakeDigitalInput.h` do not exist yet.

- [ ] **Step 3: Write minimal implementation**

Create `lib/Loco2MqttCore/src/ports/DigitalInput.h`:

```cpp
#pragma once

class DigitalInput
{
public:
    virtual ~DigitalInput() = default;
    virtual bool isActive() const = 0;
};
```

Create `test/support/FakeDigitalInput.h`:

```cpp
#pragma once

#include "ports/DigitalInput.h"

class FakeDigitalInput : public DigitalInput
{
public:
    void setActive(bool active)
    {
        active_ = active;
    }

    bool isActive() const override
    {
        return active_;
    }

private:
    bool active_ = false;
};
```

- [ ] **Step 4: Run test to verify it passes**

Run: `pio test -e native -f test_fake_digital_input`
Expected: PASS, 3/3 test cases.

- [ ] **Step 5: Commit**

```bash
git add lib/Loco2MqttCore/src/ports/DigitalInput.h test/support/FakeDigitalInput.h test/test_fake_digital_input/test_main.cpp
git commit -m "feat: add DigitalInput port and FakeDigitalInput"
```

---

### Task 8: `Clock` port + `FakeClock`

**Files:**
- Create: `lib/Loco2MqttCore/src/ports/Clock.h`
- Create: `test/support/FakeClock.h`
- Test: `test/test_fake_clock/test_main.cpp`

**Interfaces:**
- Produces: `class Clock` with `virtual unsigned long nowMilliseconds() const = 0;`. `class FakeClock : public Clock` with `void setNowMilliseconds(unsigned long)`.

- [ ] **Step 1: Write the failing test**

Create `test/test_fake_clock/test_main.cpp`:

```cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "support/FakeClock.h"

TEST_CASE("A fake clock begins at zero milliseconds")
{
    FakeClock clock;

    REQUIRE(clock.nowMilliseconds() == 0UL);
}

TEST_CASE("setNowMilliseconds changes what nowMilliseconds reports")
{
    FakeClock clock;

    clock.setNowMilliseconds(4200);

    REQUIRE(clock.nowMilliseconds() == 4200UL);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `pio test -e native -f test_fake_clock`
Expected: FAIL to compile — `ports/Clock.h` and `support/FakeClock.h` do not exist yet.

- [ ] **Step 3: Write minimal implementation**

Create `lib/Loco2MqttCore/src/ports/Clock.h`:

```cpp
#pragma once

class Clock
{
public:
    virtual ~Clock() = default;
    virtual unsigned long nowMilliseconds() const = 0;
};
```

Create `test/support/FakeClock.h`:

```cpp
#pragma once

#include "ports/Clock.h"

class FakeClock : public Clock
{
public:
    void setNowMilliseconds(unsigned long value)
    {
        now_ = value;
    }

    unsigned long nowMilliseconds() const override
    {
        return now_;
    }

private:
    unsigned long now_ = 0;
};
```

- [ ] **Step 4: Run test to verify it passes**

Run: `pio test -e native -f test_fake_clock`
Expected: PASS, 2/2 test cases.

- [ ] **Step 5: Commit**

```bash
git add lib/Loco2MqttCore/src/ports/Clock.h test/support/FakeClock.h test/test_fake_clock/test_main.cpp
git commit -m "feat: add Clock port and FakeClock"
```

---

### Task 9: `SetupModeRequestStore` port + `FakeSetupModeRequestStore`

**Files:**
- Create: `lib/Loco2MqttCore/src/ports/SetupModeRequestStore.h`
- Create: `test/support/FakeSetupModeRequestStore.h`
- Test: `test/test_fake_setup_mode_request_store/test_main.cpp`

**Interfaces:**
- Produces: `class SetupModeRequestStore` with `virtual void request() = 0;` and `virtual bool consumeIfRequested() = 0;` (reads and clears the one-shot flag in a single call). `class FakeSetupModeRequestStore : public SetupModeRequestStore`.

- [ ] **Step 1: Write the failing test**

Create `test/test_fake_setup_mode_request_store/test_main.cpp`:

```cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "support/FakeSetupModeRequestStore.h"

TEST_CASE("consumeIfRequested is false when nothing was requested")
{
    FakeSetupModeRequestStore store;

    REQUIRE(store.consumeIfRequested() == false);
}

TEST_CASE("request then consumeIfRequested reports true exactly once")
{
    FakeSetupModeRequestStore store;
    store.request();

    REQUIRE(store.consumeIfRequested() == true);
    REQUIRE(store.consumeIfRequested() == false);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `pio test -e native -f test_fake_setup_mode_request_store`
Expected: FAIL to compile — `ports/SetupModeRequestStore.h` and `support/FakeSetupModeRequestStore.h` do not exist yet.

- [ ] **Step 3: Write minimal implementation**

Create `lib/Loco2MqttCore/src/ports/SetupModeRequestStore.h`:

```cpp
#pragma once

class SetupModeRequestStore
{
public:
    virtual ~SetupModeRequestStore() = default;
    virtual void request() = 0;
    virtual bool consumeIfRequested() = 0;
};
```

Create `test/support/FakeSetupModeRequestStore.h`:

```cpp
#pragma once

#include "ports/SetupModeRequestStore.h"

class FakeSetupModeRequestStore : public SetupModeRequestStore
{
public:
    void request() override
    {
        requested_ = true;
    }

    bool consumeIfRequested() override
    {
        const bool wasRequested = requested_;
        requested_ = false;
        return wasRequested;
    }

private:
    bool requested_ = false;
};
```

- [ ] **Step 4: Run test to verify it passes**

Run: `pio test -e native -f test_fake_setup_mode_request_store`
Expected: PASS, 2/2 test cases.

- [ ] **Step 5: Commit**

```bash
git add lib/Loco2MqttCore/src/ports/SetupModeRequestStore.h test/support/FakeSetupModeRequestStore.h test/test_fake_setup_mode_request_store/test_main.cpp
git commit -m "feat: add SetupModeRequestStore port and FakeSetupModeRequestStore"
```

---

### Task 10: `ButtonSetupModeTrigger` application service

**Files:**
- Create: `lib/Loco2MqttCore/src/application/ButtonSetupModeTrigger.h`
- Create: `lib/Loco2MqttCore/src/application/ButtonSetupModeTrigger.cpp`
- Test: `test/test_button_setup_mode_trigger/test_main.cpp`

**Interfaces:**
- Consumes: `DigitalInput`/`FakeDigitalInput` (Task 7); `Clock`/`FakeClock` (Task 8); `SetupModeRequestStore`/`FakeSetupModeRequestStore` (Task 9).
- Produces: `class ButtonSetupModeTrigger` with `ButtonSetupModeTrigger(DigitalInput& button, Clock& clock, SetupModeRequestStore& requestStore)` and `bool update()` — returns `true` exactly once per continuous hold, the moment the hold reaches `kHoldDurationMs = 3000`, and calls `SetupModeRequestStore::request()` at that same moment.

- [ ] **Step 1: Write the failing test**

Create `test/test_button_setup_mode_trigger/test_main.cpp`:

```cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "application/ButtonSetupModeTrigger.h"
#include "support/FakeClock.h"
#include "support/FakeDigitalInput.h"
#include "support/FakeSetupModeRequestStore.h"

TEST_CASE("never triggers while the button is inactive")
{
    FakeDigitalInput button;
    FakeClock clock;
    FakeSetupModeRequestStore requestStore;
    ButtonSetupModeTrigger trigger(button, clock, requestStore);

    REQUIRE(trigger.update() == false);
    clock.setNowMilliseconds(5000);
    REQUIRE(trigger.update() == false);
    REQUIRE(requestStore.consumeIfRequested() == false);
}

TEST_CASE("does not trigger before the hold reaches 3000ms")
{
    FakeDigitalInput button;
    FakeClock clock;
    FakeSetupModeRequestStore requestStore;
    ButtonSetupModeTrigger trigger(button, clock, requestStore);
    button.setActive(true);

    clock.setNowMilliseconds(0);
    REQUIRE(trigger.update() == false);
    clock.setNowMilliseconds(2999);
    REQUIRE(trigger.update() == false);
    REQUIRE(requestStore.consumeIfRequested() == false);
}

TEST_CASE("triggers once a continuous hold reaches exactly 3000ms and requests setup mode")
{
    FakeDigitalInput button;
    FakeClock clock;
    FakeSetupModeRequestStore requestStore;
    ButtonSetupModeTrigger trigger(button, clock, requestStore);
    button.setActive(true);

    clock.setNowMilliseconds(0);
    trigger.update();
    clock.setNowMilliseconds(3000);

    REQUIRE(trigger.update() == true);
    REQUIRE(requestStore.consumeIfRequested() == true);
}

TEST_CASE("does not trigger again while still held past the threshold")
{
    FakeDigitalInput button;
    FakeClock clock;
    FakeSetupModeRequestStore requestStore;
    ButtonSetupModeTrigger trigger(button, clock, requestStore);
    button.setActive(true);
    clock.setNowMilliseconds(0);
    trigger.update();
    clock.setNowMilliseconds(3000);
    trigger.update();

    clock.setNowMilliseconds(4000);

    REQUIRE(trigger.update() == false);
}

TEST_CASE("releasing before the threshold resets the hold, requiring a fresh 3000ms hold")
{
    FakeDigitalInput button;
    FakeClock clock;
    FakeSetupModeRequestStore requestStore;
    ButtonSetupModeTrigger trigger(button, clock, requestStore);

    clock.setNowMilliseconds(0);
    button.setActive(true);
    trigger.update();
    clock.setNowMilliseconds(1000);
    button.setActive(false);
    trigger.update();

    clock.setNowMilliseconds(1000);
    button.setActive(true);
    trigger.update();
    clock.setNowMilliseconds(3999);
    REQUIRE(trigger.update() == false);
    clock.setNowMilliseconds(4000);
    REQUIRE(trigger.update() == true);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `pio test -e native -f test_button_setup_mode_trigger`
Expected: FAIL to compile — `application/ButtonSetupModeTrigger.h` does not exist yet.

- [ ] **Step 3: Write minimal implementation**

Create `lib/Loco2MqttCore/src/application/ButtonSetupModeTrigger.h`:

```cpp
#pragma once

#include "ports/Clock.h"
#include "ports/DigitalInput.h"
#include "ports/SetupModeRequestStore.h"

class ButtonSetupModeTrigger
{
public:
    ButtonSetupModeTrigger(DigitalInput& button, Clock& clock, SetupModeRequestStore& requestStore);

    bool update();

private:
    void resetHold();
    void beginHoldIfNeeded();
    bool triggerIfHeldLongEnough();

    DigitalInput& button_;
    Clock& clock_;
    SetupModeRequestStore& requestStore_;

    bool isHolding_ = false;
    bool alreadyTriggered_ = false;
    unsigned long holdStartMs_ = 0;

    static constexpr unsigned long kHoldDurationMs = 3000;
};
```

Create `lib/Loco2MqttCore/src/application/ButtonSetupModeTrigger.cpp`:

```cpp
#include "ButtonSetupModeTrigger.h"

ButtonSetupModeTrigger::ButtonSetupModeTrigger(DigitalInput& button, Clock& clock, SetupModeRequestStore& requestStore)
    : button_(button), clock_(clock), requestStore_(requestStore)
{
}

bool ButtonSetupModeTrigger::update()
{
    if (!button_.isActive())
    {
        resetHold();
        return false;
    }
    beginHoldIfNeeded();
    return triggerIfHeldLongEnough();
}

void ButtonSetupModeTrigger::resetHold()
{
    isHolding_ = false;
    alreadyTriggered_ = false;
}

void ButtonSetupModeTrigger::beginHoldIfNeeded()
{
    if (isHolding_)
    {
        return;
    }
    isHolding_ = true;
    holdStartMs_ = clock_.nowMilliseconds();
}

bool ButtonSetupModeTrigger::triggerIfHeldLongEnough()
{
    if (alreadyTriggered_)
    {
        return false;
    }
    if (clock_.nowMilliseconds() - holdStartMs_ < kHoldDurationMs)
    {
        return false;
    }
    requestStore_.request();
    alreadyTriggered_ = true;
    return true;
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `pio test -e native -f test_button_setup_mode_trigger`
Expected: PASS, 5/5 test cases.

- [ ] **Step 5: Commit**

```bash
git add lib/Loco2MqttCore/src/application/ButtonSetupModeTrigger.h lib/Loco2MqttCore/src/application/ButtonSetupModeTrigger.cpp test/test_button_setup_mode_trigger/test_main.cpp
git commit -m "feat: add ButtonSetupModeTrigger application service"
```

---

### Task 11: `BootMode` + `BootModeSelector`

**Files:**
- Create: `lib/Loco2MqttCore/src/domain/BootMode.h`
- Create: `lib/Loco2MqttCore/src/domain/BootMode.cpp`
- Test: `test/test_boot_mode/test_main.cpp`

**Interfaces:**
- Consumes: `LocoNetAdapterConfig` (Task 1) — `isComplete()`.
- Produces: `enum class BootMode { Normal, NeedsCommissioning, WirelessSetup };` and free function `BootMode selectBootMode(const LocoNetAdapterConfig& config, bool setupModeRequested);`.

- [ ] **Step 1: Write the failing test**

Create `test/test_boot_mode/test_main.cpp`:

```cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/BootMode.h"
#include "domain/LocoNetAdapterConfig.h"

TEST_CASE("a pending setup request wins even with a complete config")
{
    LocoNetAdapterConfig config("MyHomeWifi", "hunter2");

    REQUIRE(selectBootMode(config, true) == BootMode::WirelessSetup);
}

TEST_CASE("a pending setup request wins even with an incomplete config")
{
    LocoNetAdapterConfig config;

    REQUIRE(selectBootMode(config, true) == BootMode::WirelessSetup);
}

TEST_CASE("no setup request and an incomplete config needs commissioning")
{
    LocoNetAdapterConfig config;

    REQUIRE(selectBootMode(config, false) == BootMode::NeedsCommissioning);
}

TEST_CASE("no setup request and a complete config boots normally")
{
    LocoNetAdapterConfig config("MyHomeWifi", "hunter2");

    REQUIRE(selectBootMode(config, false) == BootMode::Normal);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `pio test -e native -f test_boot_mode`
Expected: FAIL to compile — `domain/BootMode.h` does not exist yet.

- [ ] **Step 3: Write minimal implementation**

Create `lib/Loco2MqttCore/src/domain/BootMode.h`:

```cpp
#pragma once

#include "domain/LocoNetAdapterConfig.h"

enum class BootMode
{
    Normal,
    NeedsCommissioning,
    WirelessSetup
};

BootMode selectBootMode(const LocoNetAdapterConfig& config, bool setupModeRequested);
```

Create `lib/Loco2MqttCore/src/domain/BootMode.cpp`:

```cpp
#include "BootMode.h"

BootMode selectBootMode(const LocoNetAdapterConfig& config, bool setupModeRequested)
{
    if (setupModeRequested)
    {
        return BootMode::WirelessSetup;
    }
    if (!config.isComplete())
    {
        return BootMode::NeedsCommissioning;
    }
    return BootMode::Normal;
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `pio test -e native -f test_boot_mode`
Expected: PASS, 4/4 test cases.

- [ ] **Step 5: Commit**

```bash
git add lib/Loco2MqttCore/src/domain/BootMode.h lib/Loco2MqttCore/src/domain/BootMode.cpp test/test_boot_mode/test_main.cpp
git commit -m "feat: add BootMode and BootModeSelector"
```

---

### Task 12: `SetupFormRenderer`

**Files:**
- Create: `lib/Loco2MqttCore/src/domain/SetupFormRenderer.h`
- Create: `lib/Loco2MqttCore/src/domain/SetupFormRenderer.cpp`
- Test: `test/test_setup_form_renderer/test_main.cpp`

**Interfaces:**
- Consumes: `LocoNetAdapterConfig` (Task 1) — `wifiSsid()`.
- Produces: free function `std::string renderSetupForm(const LocoNetAdapterConfig& config);`. **Security rule (Global Constraints): the password is never rendered** — the password input always has `value=""` regardless of what is stored.

- [ ] **Step 1: Write the failing test**

Create `test/test_setup_form_renderer/test_main.cpp`:

```cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/LocoNetAdapterConfig.h"
#include "domain/SetupFormRenderer.h"

TEST_CASE("renders the current SSID into the form")
{
    LocoNetAdapterConfig config("MyHomeWifi", "hunter2");

    std::string page = renderSetupForm(config);

    REQUIRE(page.find("value=\"MyHomeWifi\"") != std::string::npos);
}

TEST_CASE("never reflects the stored password back into the form")
{
    LocoNetAdapterConfig config("MyHomeWifi", "hunter2");

    std::string page = renderSetupForm(config);

    REQUIRE(page.find("hunter2") == std::string::npos);
    REQUIRE(page.find("name=\"password\"") != std::string::npos);
}

TEST_CASE("escapes special characters in the SSID so the HTML stays well-formed")
{
    LocoNetAdapterConfig config("My\"Wifi&Net", "hunter2");

    std::string page = renderSetupForm(config);

    REQUIRE(page.find("My\"Wifi&Net") == std::string::npos);
    REQUIRE(page.find("&quot;") != std::string::npos);
    REQUIRE(page.find("&amp;") != std::string::npos);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `pio test -e native -f test_setup_form_renderer`
Expected: FAIL to compile — `domain/SetupFormRenderer.h` does not exist yet.

- [ ] **Step 3: Write minimal implementation**

Create `lib/Loco2MqttCore/src/domain/SetupFormRenderer.h`:

```cpp
#pragma once

#include <string>

#include "domain/LocoNetAdapterConfig.h"

std::string renderSetupForm(const LocoNetAdapterConfig& config);
```

Create `lib/Loco2MqttCore/src/domain/SetupFormRenderer.cpp`:

```cpp
#include "SetupFormRenderer.h"

namespace
{
    void appendEscaped(std::string& out, char c)
    {
        switch (c)
        {
            case '&': out += "&amp;"; break;
            case '"': out += "&quot;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            default: out += c;
        }
    }

    std::string escapeHtmlAttribute(const std::string& value)
    {
        std::string escaped;
        for (const char c : value)
        {
            appendEscaped(escaped, c);
        }
        return escaped;
    }
}

std::string renderSetupForm(const LocoNetAdapterConfig& config)
{
    return "<html><body>"
           "<form method=\"POST\" action=\"/\">"
           "SSID: <input name=\"ssid\" value=\"" + escapeHtmlAttribute(config.wifiSsid()) + "\"><br>"
           "Password: <input name=\"password\" type=\"password\" value=\"\"><br>"
           "<input type=\"submit\" value=\"Save\">"
           "</form></body></html>";
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `pio test -e native -f test_setup_form_renderer`
Expected: PASS, 3/3 test cases.

- [ ] **Step 5: Commit**

```bash
git add lib/Loco2MqttCore/src/domain/SetupFormRenderer.h lib/Loco2MqttCore/src/domain/SetupFormRenderer.cpp test/test_setup_form_renderer/test_main.cpp
git commit -m "feat: add SetupFormRenderer"
```

---

### Task 13: `RebootTrigger` port + `WebFormCommissioningAdapter`

**Files:**
- Create: `lib/Loco2MqttCore/src/ports/RebootTrigger.h`
- Create: `test/support/FakeRebootTrigger.h`
- Create: `lib/Loco2MqttCore/src/adapters/WebFormCommissioningAdapter.h`
- Create: `lib/Loco2MqttCore/src/adapters/WebFormCommissioningAdapter.cpp`
- Test: `test/test_web_form_commissioning_adapter/test_main.cpp`

**Interfaces:**
- Consumes: `ConfigStore`/`FakeConfigStore` (Task 2, `saveCount()` included); `LocoNetAdapterConfig` (Task 1); `renderSetupForm` (Task 12).
- Produces: `class RebootTrigger` with `virtual void reboot() = 0;` and `class FakeRebootTrigger : public RebootTrigger` with `int rebootCount() const`. `class WebFormCommissioningAdapter` with `WebFormCommissioningAdapter(ConfigStore& configStore, RebootTrigger& rebootTrigger)`, `std::string renderPage() const`, and `void handleSubmission(const std::string& ssid, const std::string& password)` — saves and reboots only when the resulting config `isComplete()`; otherwise does neither.

- [ ] **Step 1: Write the failing test**

Create `test/test_web_form_commissioning_adapter/test_main.cpp`:

```cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "adapters/WebFormCommissioningAdapter.h"
#include "domain/LocoNetAdapterConfig.h"
#include "support/FakeConfigStore.h"
#include "support/FakeRebootTrigger.h"

TEST_CASE("renderPage reflects the currently stored config")
{
    FakeConfigStore store;
    FakeRebootTrigger reboot;
    store.save(LocoNetAdapterConfig("MyHomeWifi", "hunter2"));
    WebFormCommissioningAdapter adapter(store, reboot);

    std::string page = adapter.renderPage();

    REQUIRE(page.find("value=\"MyHomeWifi\"") != std::string::npos);
}

TEST_CASE("a valid submission saves the config and reboots")
{
    FakeConfigStore store;
    FakeRebootTrigger reboot;
    WebFormCommissioningAdapter adapter(store, reboot);

    adapter.handleSubmission("MyHomeWifi", "hunter2");

    REQUIRE(store.load() == LocoNetAdapterConfig("MyHomeWifi", "hunter2"));
    REQUIRE(reboot.rebootCount() == 1);
}

TEST_CASE("a submission with an empty ssid is rejected without saving or rebooting")
{
    FakeConfigStore store;
    FakeRebootTrigger reboot;
    WebFormCommissioningAdapter adapter(store, reboot);

    adapter.handleSubmission("", "hunter2");

    REQUIRE(store.saveCount() == 0);
    REQUIRE(reboot.rebootCount() == 0);
}

TEST_CASE("a submission with an empty password is rejected without saving or rebooting")
{
    FakeConfigStore store;
    FakeRebootTrigger reboot;
    WebFormCommissioningAdapter adapter(store, reboot);

    adapter.handleSubmission("MyHomeWifi", "");

    REQUIRE(store.saveCount() == 0);
    REQUIRE(reboot.rebootCount() == 0);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `pio test -e native -f test_web_form_commissioning_adapter`
Expected: FAIL to compile — `ports/RebootTrigger.h` and `adapters/WebFormCommissioningAdapter.h` do not exist yet.

- [ ] **Step 3: Write minimal implementation**

Create `lib/Loco2MqttCore/src/ports/RebootTrigger.h`:

```cpp
#pragma once

class RebootTrigger
{
public:
    virtual ~RebootTrigger() = default;
    virtual void reboot() = 0;
};
```

Create `test/support/FakeRebootTrigger.h`:

```cpp
#pragma once

#include "ports/RebootTrigger.h"

class FakeRebootTrigger : public RebootTrigger
{
public:
    void reboot() override
    {
        rebootCount_++;
    }

    int rebootCount() const
    {
        return rebootCount_;
    }

private:
    int rebootCount_ = 0;
};
```

Create `lib/Loco2MqttCore/src/adapters/WebFormCommissioningAdapter.h`:

```cpp
#pragma once

#include <string>

#include "domain/LocoNetAdapterConfig.h"
#include "ports/ConfigStore.h"
#include "ports/RebootTrigger.h"

class WebFormCommissioningAdapter
{
public:
    WebFormCommissioningAdapter(ConfigStore& configStore, RebootTrigger& rebootTrigger);

    std::string renderPage() const;
    void handleSubmission(const std::string& ssid, const std::string& password);

private:
    ConfigStore& configStore_;
    RebootTrigger& rebootTrigger_;
};
```

Create `lib/Loco2MqttCore/src/adapters/WebFormCommissioningAdapter.cpp`:

```cpp
#include "WebFormCommissioningAdapter.h"

#include "domain/SetupFormRenderer.h"

WebFormCommissioningAdapter::WebFormCommissioningAdapter(ConfigStore& configStore, RebootTrigger& rebootTrigger)
    : configStore_(configStore), rebootTrigger_(rebootTrigger)
{
}

std::string WebFormCommissioningAdapter::renderPage() const
{
    return renderSetupForm(configStore_.load());
}

void WebFormCommissioningAdapter::handleSubmission(const std::string& ssid, const std::string& password)
{
    const LocoNetAdapterConfig config(ssid, password);
    if (!config.isComplete())
    {
        return;
    }
    configStore_.save(config);
    rebootTrigger_.reboot();
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `pio test -e native -f test_web_form_commissioning_adapter`
Expected: PASS, 4/4 test cases.

- [ ] **Step 5: Commit**

```bash
git add lib/Loco2MqttCore/src/ports/RebootTrigger.h test/support/FakeRebootTrigger.h lib/Loco2MqttCore/src/adapters/WebFormCommissioningAdapter.h lib/Loco2MqttCore/src/adapters/WebFormCommissioningAdapter.cpp test/test_web_form_commissioning_adapter/test_main.cpp
git commit -m "feat: add RebootTrigger port and WebFormCommissioningAdapter"
```

---

### Task 14: ESP32 storage/IO hardware adapters

**Files:**
- Create: `lib/Loco2MqttCore/src/adapters/NvsConfigStore.h`
- Create: `lib/Loco2MqttCore/src/adapters/NvsConfigStore.cpp`
- Create: `lib/Loco2MqttCore/src/adapters/EspUartPort.h`
- Create: `lib/Loco2MqttCore/src/adapters/EspUartPort.cpp`
- Create: `lib/Loco2MqttCore/src/adapters/EspDigitalInput.h`
- Create: `lib/Loco2MqttCore/src/adapters/ArduinoClock.h`
- Create: `lib/Loco2MqttCore/src/adapters/NvsSetupModeRequestStore.h`
- Create: `lib/Loco2MqttCore/src/adapters/NvsSetupModeRequestStore.cpp`
- Create: `lib/Loco2MqttCore/src/adapters/EspRebootTrigger.h`

**Interfaces:**
- Consumes: `ConfigStore` (Task 2), `UartPort` (Task 5), `DigitalInput` (Task 7), `Clock` (Task 8), `SetupModeRequestStore` (Task 9), `RebootTrigger` (Task 13).
- Produces: `NvsConfigStore`, `EspUartPort`, `EspDigitalInput`, `ArduinoClock`, `NvsSetupModeRequestStore`, `EspRebootTrigger` — one real ESP32 implementation of each port above. No native test; verified by a full `esp32dev` build.

This task has no native-testable logic (every class is a thin wrapper around Arduino/ESP32 APIs), so there is no RED step — go straight to implementation, matching this project's existing convention for `EspDigitalPin`/`SerialMessageLog`.

- [ ] **Step 1: Write the adapters**

Create `lib/Loco2MqttCore/src/adapters/NvsConfigStore.h`:

```cpp
#pragma once

#ifdef ARDUINO

#include "ports/ConfigStore.h"

class NvsConfigStore final : public ConfigStore
{
public:
    LocoNetAdapterConfig load() override;
    void save(const LocoNetAdapterConfig& config) override;
};

#endif
```

Create `lib/Loco2MqttCore/src/adapters/NvsConfigStore.cpp`:

```cpp
#ifdef ARDUINO

#include "NvsConfigStore.h"

#include <Preferences.h>

namespace
{
    constexpr const char* kNamespace = "loco2mqtt";
    constexpr const char* kSsidKey = "ssid";
    constexpr const char* kPasswordKey = "password";
}

LocoNetAdapterConfig NvsConfigStore::load()
{
    Preferences preferences;
    preferences.begin(kNamespace, /* readOnly = */ true);
    const std::string ssid = preferences.getString(kSsidKey, "").c_str();
    const std::string password = preferences.getString(kPasswordKey, "").c_str();
    preferences.end();
    return LocoNetAdapterConfig(ssid, password);
}

void NvsConfigStore::save(const LocoNetAdapterConfig& config)
{
    Preferences preferences;
    preferences.begin(kNamespace, /* readOnly = */ false);
    preferences.putString(kSsidKey, config.wifiSsid().c_str());
    preferences.putString(kPasswordKey, config.wifiPassword().c_str());
    preferences.end();
}

#endif
```

Create `lib/Loco2MqttCore/src/adapters/EspUartPort.h`:

```cpp
#pragma once

#ifdef ARDUINO

#include <optional>
#include <string>

#include "ports/UartPort.h"

class EspUartPort final : public UartPort
{
public:
    std::optional<std::string> readLine() override;
    void writeLine(const std::string& line) override;

private:
    // Bounds worst-case heap growth if a client sends bytes with no
    // newline; SerialCommissioningAdapter separately rejects any
    // completed line over its own kMaxLineLength.
    static constexpr std::size_t kMaxBufferedBytes = 256;

    std::string buffer_;
};

#endif
```

Create `lib/Loco2MqttCore/src/adapters/EspUartPort.cpp`:

```cpp
#ifdef ARDUINO

#include "EspUartPort.h"

#include <Arduino.h>

std::optional<std::string> EspUartPort::readLine()
{
    while (Serial.available() > 0)
    {
        const char c = static_cast<char>(Serial.read());
        if (c == '\n')
        {
            std::string line = buffer_;
            buffer_.clear();
            if (!line.empty() && line.back() == '\r')
            {
                line.pop_back();
            }
            return line;
        }
        if (buffer_.size() < kMaxBufferedBytes)
        {
            buffer_ += c;
        }
    }
    return std::nullopt;
}

void EspUartPort::writeLine(const std::string& line)
{
    Serial.println(line.c_str());
}

#endif
```

Create `lib/Loco2MqttCore/src/adapters/EspDigitalInput.h`:

```cpp
#pragma once

#ifdef ARDUINO

#include <Arduino.h>

#include "ports/DigitalInput.h"

class EspDigitalInput final : public DigitalInput
{
public:
    explicit EspDigitalInput(int pin) : pin_(pin)
    {
        pinMode(pin_, INPUT_PULLUP);
    }

    bool isActive() const override
    {
        return digitalRead(pin_) == LOW;
    }

private:
    int pin_;
};

#endif
```

Create `lib/Loco2MqttCore/src/adapters/ArduinoClock.h`:

```cpp
#pragma once

#ifdef ARDUINO

#include <Arduino.h>

#include "ports/Clock.h"

class ArduinoClock final : public Clock
{
public:
    unsigned long nowMilliseconds() const override
    {
        return millis();
    }
};

#endif
```

Create `lib/Loco2MqttCore/src/adapters/NvsSetupModeRequestStore.h`:

```cpp
#pragma once

#ifdef ARDUINO

#include "ports/SetupModeRequestStore.h"

class NvsSetupModeRequestStore final : public SetupModeRequestStore
{
public:
    void request() override;
    bool consumeIfRequested() override;
};

#endif
```

Create `lib/Loco2MqttCore/src/adapters/NvsSetupModeRequestStore.cpp`:

```cpp
#ifdef ARDUINO

#include "NvsSetupModeRequestStore.h"

#include <Preferences.h>

namespace
{
    constexpr const char* kNamespace = "loco2mqtt";
    constexpr const char* kRequestedKey = "setup_req";
}

void NvsSetupModeRequestStore::request()
{
    Preferences preferences;
    preferences.begin(kNamespace, /* readOnly = */ false);
    preferences.putBool(kRequestedKey, true);
    preferences.end();
}

bool NvsSetupModeRequestStore::consumeIfRequested()
{
    Preferences preferences;
    preferences.begin(kNamespace, /* readOnly = */ false);
    const bool wasRequested = preferences.getBool(kRequestedKey, false);
    preferences.putBool(kRequestedKey, false);
    preferences.end();
    return wasRequested;
}

#endif
```

Create `lib/Loco2MqttCore/src/adapters/EspRebootTrigger.h`:

```cpp
#pragma once

#ifdef ARDUINO

#include <Arduino.h>

#include "ports/RebootTrigger.h"

class EspRebootTrigger final : public RebootTrigger
{
public:
    void reboot() override
    {
        ESP.restart();
    }
};

#endif
```

- [ ] **Step 2: Build-check against the real ESP32 toolchain**

Run: `pio run -e esp32dev`
Expected: `SUCCESS`. (Native tests are unaffected — none of these files are reachable from any `test/` file, and `native`'s `build_flags` doesn't define `ARDUINO`, so their entire contents are preprocessed out under `native`.)

- [ ] **Step 3: Run the native suite to confirm no regression**

Run: `pio test -e native`
Expected: PASS, all suites (unchanged from before this task).

- [ ] **Step 4: Commit**

```bash
git add lib/Loco2MqttCore/src/adapters/NvsConfigStore.h lib/Loco2MqttCore/src/adapters/NvsConfigStore.cpp lib/Loco2MqttCore/src/adapters/EspUartPort.h lib/Loco2MqttCore/src/adapters/EspUartPort.cpp lib/Loco2MqttCore/src/adapters/EspDigitalInput.h lib/Loco2MqttCore/src/adapters/ArduinoClock.h lib/Loco2MqttCore/src/adapters/NvsSetupModeRequestStore.h lib/Loco2MqttCore/src/adapters/NvsSetupModeRequestStore.cpp lib/Loco2MqttCore/src/adapters/EspRebootTrigger.h
git commit -m "feat: add ESP32 storage/IO hardware adapters for commissioning"
```

---

### Task 15: `CaptivePortalServer`

**Files:**
- Create: `lib/Loco2MqttCore/src/adapters/CaptivePortalServer.h`
- Create: `lib/Loco2MqttCore/src/adapters/CaptivePortalServer.cpp`

**Interfaces:**
- Consumes: `WebFormCommissioningAdapter` (Task 13) — `renderPage()`, `handleSubmission()`.
- Produces: `class CaptivePortalServer` with `explicit CaptivePortalServer(WebFormCommissioningAdapter& formAdapter)`, `void begin()` (starts the open AP `"Loco2MQTT-Setup"`, the DNS-capture server, and the web server's routes), and `void update()` (services one DNS + one HTTP tick — called from `loop()`). No native test — this is a genuine hardware/library shim, build-check only.

- [ ] **Step 1: Write the adapter**

Create `lib/Loco2MqttCore/src/adapters/CaptivePortalServer.h`:

```cpp
#pragma once

#ifdef ARDUINO

#include <cstdint>

#include <DNSServer.h>
#include <WebServer.h>

#include "adapters/WebFormCommissioningAdapter.h"

class CaptivePortalServer
{
public:
    explicit CaptivePortalServer(WebFormCommissioningAdapter& formAdapter);

    void begin();
    void update();

private:
    void handleRoot();
    void handleSubmit();

    WebFormCommissioningAdapter& formAdapter_;
    DNSServer dnsServer_;
    WebServer webServer_;

    static constexpr const char* kApName = "Loco2MQTT-Setup";
    static constexpr uint8_t kDnsPort = 53;
};

#endif
```

Create `lib/Loco2MqttCore/src/adapters/CaptivePortalServer.cpp`:

```cpp
#ifdef ARDUINO

#include "CaptivePortalServer.h"

#include <WiFi.h>

CaptivePortalServer::CaptivePortalServer(WebFormCommissioningAdapter& formAdapter)
    : formAdapter_(formAdapter), webServer_(80)
{
}

void CaptivePortalServer::begin()
{
    WiFi.softAP(kApName);
    dnsServer_.start(kDnsPort, "*", WiFi.softAPIP());
    webServer_.on("/", HTTP_GET, [this]() { handleRoot(); });
    webServer_.on("/", HTTP_POST, [this]() { handleSubmit(); });
    webServer_.begin();
}

void CaptivePortalServer::update()
{
    dnsServer_.processNextRequest();
    webServer_.handleClient();
}

void CaptivePortalServer::handleRoot()
{
    webServer_.send(200, "text/html", formAdapter_.renderPage().c_str());
}

void CaptivePortalServer::handleSubmit()
{
    const std::string ssid = webServer_.arg("ssid").c_str();
    const std::string password = webServer_.arg("password").c_str();
    formAdapter_.handleSubmission(ssid, password);
    webServer_.send(200, "text/html", "Saved. Rebooting...");
}

#endif
```

- [ ] **Step 2: Build-check against the real ESP32 toolchain**

Run: `pio run -e esp32dev`
Expected: `SUCCESS`.

- [ ] **Step 3: Run the native suite to confirm no regression**

Run: `pio test -e native`
Expected: PASS, all suites (unchanged from before this task).

- [ ] **Step 4: Commit**

```bash
git add lib/Loco2MqttCore/src/adapters/CaptivePortalServer.h lib/Loco2MqttCore/src/adapters/CaptivePortalServer.cpp
git commit -m "feat: add CaptivePortalServer adapter"
```

---

### Task 16: Composition root wiring

**Files:**
- Modify: `src/main.cpp` (full rewrite — see below)
- Modify: `CLAUDE.md` (`## Current source layout` section)
- Modify: `README.md` (`## What it doesn't do yet` and a new `## WiFi commissioning` section)

**Interfaces:**
- Consumes every class produced by Tasks 1–15.

This task has no native test (`src/main.cpp` is excluded from the `native` build by `test_build_src = false`) — verified by an `esp32dev` build plus a re-run of the full native suite to confirm no regression.

- [ ] **Step 1: Rewrite the composition root**

Replace the full contents of `src/main.cpp` with:

```cpp
#include <Arduino.h>
#include <optional>

#include "adapters/ArduinoClock.h"
#include "adapters/CaptivePortalServer.h"
#include "adapters/EspDigitalInput.h"
#include "adapters/EspRebootTrigger.h"
#include "adapters/EspUartPort.h"
#include "adapters/LocoNetEsp32Port.h"
#include "adapters/NvsConfigStore.h"
#include "adapters/NvsSetupModeRequestStore.h"
#include "adapters/SerialCommissioningAdapter.h"
#include "adapters/SerialMessageLog.h"
#include "adapters/WebFormCommissioningAdapter.h"
#include "application/ButtonSetupModeTrigger.h"
#include "application/CommissioningSession.h"
#include "application/LocoNetMessageLogger.h"
#include "domain/BootMode.h"

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
ArduinoClock clock;
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

namespace
{
    void setupNormalOrNeedsCommissioning()
    {
        locoNetPort.emplace(kLocoNetRxPin, kLocoNetTxPin);
        logger.emplace(*locoNetPort, messageLog);
        setupModeTrigger.emplace(bootButton, clock, setupModeRequestStore);
        if (bootMode == BootMode::NeedsCommissioning)
        {
            commissioningSession.emplace(configStore);
            serialCommissioning.emplace(uartPort, *commissioningSession);
        }
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
    }
}
```

Note for the implementer: this changes *when* `LocoNetEsp32Port`'s constructor runs relative to `Serial.begin()` — previously it ran as part of global-initializer order, before `setup()` started; now it runs inside `setup()`, after `Serial.begin()`, via `.emplace()`. The vendor self-init behavior documented in `CLAUDE.md` (constructor calls its own `begin()` whenever either pin is non-negative) is unaffected — only the timing relative to `Serial.begin()` changes, which is a strict improvement (serial is available for any startup diagnostics the vendor library might emit). `LocoNetEsp32Port::begin()` remains the documented no-op it already is and is still never called.

- [ ] **Step 2: Update CLAUDE.md's source layout section**

In `CLAUDE.md`, find the `### Current source layout` section and replace it with:

```markdown
### Current source layout

- `lib/Loco2MqttCore/src/domain/` — `Level`, `LocoNetMessage`,
  `LocoNetAdapterConfig`, `ParsedCommand`, `CommandLineParser`, `BootMode`
  (+ `BootModeSelector`), `SetupFormRenderer`
- `lib/Loco2MqttCore/src/ports/` — `DigitalPin`, `LocoNetPort`,
  `MessageLog`, `ConfigStore`, `UartPort`, `DigitalInput`, `Clock`,
  `SetupModeRequestStore`, `RebootTrigger`
- `lib/Loco2MqttCore/src/application/` — `LocoNetMessageLogger`,
  `CommissioningSession`, `ButtonSetupModeTrigger`
- `lib/Loco2MqttCore/src/adapters/` — `EspDigitalPin`, `LocoNetEsp32Port`,
  `SerialMessageLog`, `NvsConfigStore`, `EspUartPort`,
  `SerialCommissioningAdapter`, `EspDigitalInput`, `ArduinoClock`,
  `NvsSetupModeRequestStore`, `EspRebootTrigger`,
  `WebFormCommissioningAdapter`, `CaptivePortalServer`
- `test/support/` — `FakeDigitalPin`, `FakeLocoNetPort`, `FakeMessageLog`,
  `FakeConfigStore`, `FakeUartPort`, `FakeDigitalInput`, `FakeClock`,
  `FakeSetupModeRequestStore`, `FakeRebootTrigger`
- `test/test_<name>/test_main.cpp` — Catch2 test binaries
```

- [ ] **Step 3: Update README.md**

In `README.md`, in the `## What it doesn't do yet` section, replace this line:

```markdown
- No MQTT client, no broker connection, no topic scheme.
```

with:

```markdown
- No MQTT client, no broker connection, no topic scheme. (WiFi credentials
  *are* now configurable — see "WiFi commissioning" below — but nothing
  yet uses the network they connect to.)
```

Then, immediately after the `## Configuration reference` section (before `## Architecture overview`), add:

```markdown
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
```

- [ ] **Step 4: Build-check against the real ESP32 toolchain**

Run: `pio run -e esp32dev`
Expected: `SUCCESS`.

- [ ] **Step 5: Run the full native suite**

Run: `pio test -e native`
Expected: PASS, all suites (this task adds no new native tests of its own since `src/main.cpp` isn't natively built).

- [ ] **Step 6: Commit**

```bash
git add src/main.cpp CLAUDE.md README.md
git commit -m "feat: wire WiFi commissioning into the composition root"
```

---

## Self-Review

**Spec coverage:**
- Section 1 (Scope) → Task 16 (both front doors wired, existing slice untouched in Normal/NeedsCommissioning).
- Section 2 (Config domain object) → Tasks 1–2.
- Section 3 (Bench-serial commissioning) → Tasks 3–6.
- Section 4 (Wireless setup trigger) → Tasks 7–10.
- Section 5 (Captive portal) → Tasks 11–13, 15.
- Section 6 (Composition root wiring) → Task 16.
- Section 7 (Testing & error handling) → covered inline in every task; hardware shims correctly build-check-only in Tasks 14–15.
- Section 8 (File layout) → matches exactly; the only addition is `RebootTrigger`/`FakeRebootTrigger`, explained in the plan header.
- Section 9 (Out of scope) → nothing in this plan touches MQTT, PicoMQTT, mDNS, or topic routing.

**Placeholder scan:** no TBD/TODO; every step has complete, runnable code.

**Type consistency:** checked `ConfigStore`, `LocoNetAdapterConfig`, `ParsedCommand`/`CommandType`, `UartPort`, `DigitalInput`, `Clock`, `SetupModeRequestStore`, `RebootTrigger`, `BootMode`/`selectBootMode`, `renderSetupForm`, and `WebFormCommissioningAdapter` signatures are identical everywhere they're consumed across tasks (Task 4 vs. Task 6's use of `CommissioningSession`; Task 10's use of the three Task 7–9 ports; Task 13/15's use of `WebFormCommissioningAdapter`; Task 16's use of every class) — no naming drift found.

---

**Plan complete and saved to `docs/superpowers/plans/2026-09-05-node-config-commissioning.md`. Two execution options:**

**1. Subagent-Driven (recommended)** - I dispatch a fresh subagent per task, review between tasks, fast iteration

**2. Inline Execution** - Execute tasks in this session using executing-plans, batch execution with checkpoints

**Which approach?**
