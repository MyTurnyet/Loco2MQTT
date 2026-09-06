#include <Arduino.h>
#include <optional>

#include "adapters/ArduinoClock.h"
#include "adapters/CaptivePortalServer.h"
#include "adapters/EspDigitalInput.h"
#include "adapters/EspMdnsPort.h"
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
#include "domain/FirmwareVersion.h"
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

    // No `.local` suffix — the mDNS responder answers under that domain
    // for whatever hostname is registered, without it being part of the
    // value passed to MDNS.begin().
    constexpr const char* kMdnsHostname = "loco2mqtt";

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

namespace
{
    void setupMqttBridge(const LocoNetAdapterConfig& config)
    {
        wifiPort.emplace(config.wifiSsid(), config.wifiPassword());
        mqttPort.emplace();
        mdnsPort.emplace();
        sendScheduler.emplace(*locoNetPort, systemClock);
        turnoutLocoNetDecoder.emplace();
        turnoutMqttEncoder.emplace();
        turnoutMqttCommandDecoder.emplace();
        turnoutLocoNetEncoder.emplace();
        // LocoNetMessageRouter owns the sole drain of locoNetPort->receive()
        // in this mode and records every message to messageLog itself — see
        // its constructor comment. A separate LocoNetMessageLogger is never
        // constructed here, so there's no second consumer racing it for the
        // same destructive-read queue.
        locoNetMessageRouter.emplace(*locoNetPort, *mqttPort, systemClock, messageLog,
                                      std::vector<std::pair<LocoNetMessageDecoder*, MqttEventEncoder*>>{
                                          {&*turnoutLocoNetDecoder, &*turnoutMqttEncoder}});
        mqttCommandRouter.emplace(*mqttPort, *sendScheduler,
                                   std::vector<std::pair<MqttCommandDecoder*, LocoNetEncoder*>>{
                                       {&*turnoutMqttCommandDecoder, &*turnoutLocoNetEncoder}});
        // mqttPort->begin() is deferred to loop() until wifiPort reports a
        // real connection — EspWifiPort::update() doesn't even issue its
        // first WiFi.begin() until loop() runs, so WiFi is guaranteed not
        // connected yet at this point in setup().
    }

    void setupNormalOrNeedsCommissioning()
    {
        locoNetPort.emplace(kLocoNetRxPin, kLocoNetTxPin);
        setupModeTrigger.emplace(bootButton, systemClock, setupModeRequestStore);
        if (bootMode == BootMode::NeedsCommissioning)
        {
            logger.emplace(*locoNetPort, messageLog);
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
    Serial.print("Loco2MQTT v");
    Serial.println(kFirmwareVersion);
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
    if (setupModeTrigger->update())
    {
        ESP.restart();
    }
    if (bootMode == BootMode::NeedsCommissioning)
    {
        logger->update();
        serialCommissioning->update();
        return;
    }
    wifiPort->update();
    if (!mqttBegun && wifiPort->isConnected())
    {
        mqttPort->begin();
        mqttBegun = true;
    }
    if (!mdnsBegun && wifiPort->isConnected())
    {
        // Only latches once begin() actually succeeds; a failed attempt
        // (e.g. transient mdns_init() failure) retries on the next tick
        // rather than silently giving up for the rest of this boot.
        mdnsBegun = mdnsPort->begin(kMdnsHostname);
    }
    mqttPort->update();
    sendScheduler->update();
    locoNetMessageRouter->update();
    mqttCommandRouter->update();
}
