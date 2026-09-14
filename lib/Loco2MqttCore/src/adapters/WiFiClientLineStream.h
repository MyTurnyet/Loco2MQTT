#pragma once

#ifdef ARDUINO

#include <WiFi.h>

#include <cstdint>
#include <optional>
#include <string>

#include "domain/LineAssembler.h"
#include "ports/LineStream.h"

// LineStream backed by a WiFiClient TCP socket to JMRI's "Start LocoNet
// Server" (LocoNetOverTcp). Thin, Arduino-guarded, build-check only — same
// tier as LocoNetEsp32Port/EspUartPort, not natively tested.
//
// Frames lines on '\n', stripping a trailing '\r' — JMRI actually sends
// '\r\n', not the bare '\r' originally assumed (see the "Addendum" in
// docs/decisions/0001-interim-jmri-loconet-over-tcp-transport.md for the
// correction). Uses LineAssembler's default '\n' terminator, the same
// pattern EspUartPort already uses — the earlier claim that this matched
// EspUartPort was itself wrong; EspUartPort was never on '\r'.
class WiFiClientLineStream final : public LineStream
{
public:
    WiFiClientLineStream(std::string host, uint16_t port);

    std::optional<std::string> readLine() override;
    void writeLine(const std::string& line) override;
    bool isConnected() const override;

private:
    void reconnectIfDue();
    bool isReconnectDue() const;

    std::string host_;
    uint16_t port_;
    // mutable: WiFiClient::connected() is non-const in this framework
    // version despite being a status query, not a logical mutation —
    // needed to keep isConnected() const per the LineStream contract.
    mutable WiFiClient client_;
    unsigned long lastConnectAttemptAtMillis_ = 0;

    // Bounds worst-case heap growth if JMRI is ever silent for a very
    // long "line" with no '\r'. Mirrors EspUartPort's kMaxBufferedBytes.
    static constexpr std::size_t kMaxBufferedBytes = 256;

    LineAssembler assembler_{kMaxBufferedBytes};

    // Mirrors EspWifiPort's retry cadence for the same reason: avoid
    // hammering connect() every single loop() tick while JMRI is down.
    static constexpr unsigned long kReconnectIntervalMs = 5000;
};

#endif
