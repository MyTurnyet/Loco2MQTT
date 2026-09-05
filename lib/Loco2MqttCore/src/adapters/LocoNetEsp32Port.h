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
