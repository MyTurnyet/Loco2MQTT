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
    // locoNetPort's hardware is already initialized by LocoNetESPSerial's
    // constructor (which self-calls begin() when both pins are
    // non-negative); LocoNetEsp32Port::begin() is a documented no-op, so it
    // is intentionally not called here to avoid a second hardware init.
}

void loop()
{
    locoNetPort.update();
    logger.update();
}
