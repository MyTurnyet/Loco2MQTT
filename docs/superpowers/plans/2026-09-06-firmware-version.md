# Firmware Version Number Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a single manually-bumped firmware version constant, shown on the wireless-setup AP configuration page and printed once to the serial console at boot.

**Architecture:** One new header-only domain constant (`kFirmwareVersion`) is the single source of truth. `SetupFormRenderer` (an existing pure, natively-testable domain function) renders it into the AP config page's HTML; `src/main.cpp` (the composition root) prints it to `Serial` once at the top of `setup()`.

**Tech Stack:** C++17, PlatformIO/Arduino, Catch2 (native tests only — `src/main.cpp` has no native test target).

## Global Constraints

- The version is a manually-maintained semantic version string, starting at `"0.1.0"` — no build tooling, no git-derived value.
- `kFirmwareVersion` lives in `lib/Loco2MqttCore/src/domain/FirmwareVersion.h` as a header-only `constexpr const char*` — no `.cpp`, no dependencies.
- Shown in exactly two places: the AP config page's rendered HTML, and one `Serial.println` at the very top of `setup()` in `src/main.cpp`, immediately after `Serial.begin(kSerialBaudRate)` — before `selectBootMode()` or any other setup work.
- No git-derived version, no display anywhere else (README, MQTT topics, HTTP headers), no changelog/version-history tracking.

---

### Task 1: Add and wire up the firmware version constant

**Files:**
- Create: `lib/Loco2MqttCore/src/domain/FirmwareVersion.h`
- Modify: `lib/Loco2MqttCore/src/domain/SetupFormRenderer.cpp`
- Modify: `test/test_setup_form_renderer/test_main.cpp`
- Modify: `src/main.cpp`

**Interfaces:**
- Produces: `constexpr const char* kFirmwareVersion` (declared and defined together in the header, value `"0.1.0"`) — consumed directly by both `SetupFormRenderer.cpp` and `src/main.cpp` via `#include "domain/FirmwareVersion.h"`.

This is one task because both consumers of `kFirmwareVersion` are small, mechanical additions with no meaningful independent test cycle of their own — the AP-page change is covered by a native test, and the boot-print change is build-check-only, matching how every other `src/main.cpp` change in this project has been verified.

- [ ] **Step 1: Create the version header**

```cpp
// lib/Loco2MqttCore/src/domain/FirmwareVersion.h
#pragma once

constexpr const char* kFirmwareVersion = "0.1.0";
```

- [ ] **Step 2: Write the failing test for the AP config page**

Open `test/test_setup_form_renderer/test_main.cpp` and add this test case (after the existing three, before the file's closing):

```cpp
TEST_CASE("shows the firmware version on the page")
{
    LocoNetAdapterConfig config("MyHomeWifi", "hunter2");

    std::string page = renderSetupForm(config);

    REQUIRE(page.find(kFirmwareVersion) != std::string::npos);
}
```

Also add the new include near the top of the file, alongside the existing two:

```cpp
#include "domain/FirmwareVersion.h"
#include "domain/LocoNetAdapterConfig.h"
#include "domain/SetupFormRenderer.h"
```

- [ ] **Step 3: Run the test to verify it fails**

Run: `pio test -e native -f test_setup_form_renderer`
Expected: FAIL — `kFirmwareVersion` is undeclared until Step 1's header is included by the test file (it will compile once Step 1 exists, but the `REQUIRE` will fail since `renderSetupForm` doesn't render the version yet).

- [ ] **Step 4: Render the version into the AP config page**

Open `lib/Loco2MqttCore/src/domain/SetupFormRenderer.cpp`. Add the include:

```cpp
#include "SetupFormRenderer.h"

#include "domain/FirmwareVersion.h"
```

Then change the `renderSetupForm` function from:

```cpp
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

to:

```cpp
std::string renderSetupForm(const LocoNetAdapterConfig& config)
{
    return "<html><body>"
           "<p>Loco2MQTT v" + std::string(kFirmwareVersion) + "</p>"
           "<form method=\"POST\" action=\"/\">"
           "SSID: <input name=\"ssid\" value=\"" + escapeHtmlAttribute(config.wifiSsid()) + "\"><br>"
           "Password: <input name=\"password\" type=\"password\" value=\"\"><br>"
           "<input type=\"submit\" value=\"Save\">"
           "</form></body></html>";
}
```

- [ ] **Step 5: Run the test to verify it passes**

Run: `pio test -e native -f test_setup_form_renderer`
Expected: PASS (4 test cases, up from 3).

- [ ] **Step 6: Run the full native suite to confirm nothing else broke**

Run: `pio test -e native`
Expected: all suites still pass (33 suites before this task; still 33 suites, one more test case in `test_setup_form_renderer`).

- [ ] **Step 7: Print the version to serial at boot**

Open `src/main.cpp`. Add the include in alphabetical order among the existing `domain/` includes:

```cpp
#include "domain/BootMode.h"
#include "domain/FirmwareVersion.h"
```

Then change `setup()` from:

```cpp
void setup()
{
    Serial.begin(kSerialBaudRate);
    bootMode = selectBootMode(configStore.load(), setupModeRequestStore.consumeIfRequested());
```

to:

```cpp
void setup()
{
    Serial.begin(kSerialBaudRate);
    Serial.print("Loco2MQTT v");
    Serial.println(kFirmwareVersion);
    bootMode = selectBootMode(configStore.load(), setupModeRequestStore.consumeIfRequested());
```

- [ ] **Step 8: Build-check against the ESP32 target**

Run: `pio run -e esp32dev`
Expected: `SUCCESS` — this is the only verification for Step 7's change, since `src/main.cpp` has no native test target (`test_build_src = false` in `platformio.ini`).

- [ ] **Step 9: Commit**

```bash
git add lib/Loco2MqttCore/src/domain/FirmwareVersion.h lib/Loco2MqttCore/src/domain/SetupFormRenderer.cpp test/test_setup_form_renderer/test_main.cpp src/main.cpp
git commit -m "feat: add firmware version, shown on AP config page and at boot"
```

---

## Self-Review Notes

- **Spec coverage:** the header (Format + Component sections), the AP-page display, and the serial boot-print are all covered by Task 1's steps.
- **Placeholder scan:** no TBDs; every step has complete code.
- **Type consistency:** `kFirmwareVersion` is declared once in Step 1 (`constexpr const char*`) and used identically (as a `const char*`, wrapped in `std::string(...)` only where concatenation requires it) in both consumers.
- **Explicitly out of scope** (git-derived version, other display locations, changelog) is not touched anywhere in this task.
