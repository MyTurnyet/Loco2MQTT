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
