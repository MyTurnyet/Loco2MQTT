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
