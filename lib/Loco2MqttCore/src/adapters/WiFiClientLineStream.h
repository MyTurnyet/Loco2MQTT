#pragma once

#ifdef ARDUINO

#include <WiFi.h>

#include <cstdint>
#include <optional>
#include <string>

#include "ports/LineStream.h"

// LineStream backed by a WiFiClient TCP socket to JMRI's "Start LocoNet
// Server" (LocoNetOverTcp). Thin, Arduino-guarded, build-check only — same
// tier as LocoNetEsp32Port/EspUartPort, not natively tested.
//
// Frames lines on a bare '\r' — confirmed empirically against a real JMRI
// 5.2 session (see docs/decisions/0001-interim-jmri-loconet-over-tcp-transport.md),
// which sends neither '\n' nor '\r\n'. This is its own small loop rather
// than a reuse of LineAssembler, which only recognizes '\n' as a
// terminator and is shared with the USB-serial commissioning path — a
// deliberate choice to avoid touching that already-tested, already-shipped
// class for a one-off protocol quirk.
class WiFiClientLineStream final : public LineStream
{
public:
    WiFiClientLineStream(std::string host, uint16_t port);

    std::optional<std::string> readLine() override;
    void writeLine(const std::string& line) override;
    bool isConnected() const override;

private:
    void reconnectIfDue();
    std::optional<std::string> finishLineIfComplete(char c);

    std::string host_;
    uint16_t port_;
    WiFiClient client_;
    std::string buffer_;
    unsigned long lastConnectAttemptAtMillis_ = 0;

    // Mirrors EspUartPort's kMaxBufferedBytes: bounds worst-case heap
    // growth if JMRI is ever silent for a very long "line" with no '\r'.
    static constexpr std::size_t kMaxBufferedBytes = 256;

    // Mirrors EspWifiPort's retry cadence for the same reason: avoid
    // hammering connect() every single loop() tick while JMRI is down.
    static constexpr unsigned long kReconnectIntervalMs = 5000;
};

#endif
