#ifdef ARDUINO

#include "LocoNetEsp32Port.h"

#include <algorithm>
#include <queue>
#include <vector>

namespace
{
    // Modest cap on the receive queue: LocoNet's own bus speed (16.66kbps,
    // roughly one short message per few milliseconds at saturation) means a
    // healthy layout produces far fewer than this between two loop() ticks.
    // 32 gives ample headroom for a burst while still bounding worst-case
    // heap growth if a future blocking call (e.g. MQTT publish) stalls
    // draining for a while. Oldest entries are dropped first so the queue
    // always reflects the most recent bus activity.
    constexpr std::size_t kMaxPendingMessages = 32;

    std::queue<std::vector<uint8_t>>& pendingMessages()
    {
        static std::queue<std::vector<uint8_t>> queue;
        return queue;
    }

    // Bits in lnReceiveBuffer::errorFlags (IoTTCommDef.h) that mark a frame
    // as not valid received data: transmit-path errors (collision/frame/
    // timeout/carrier-loss — currently dead code in the vendor library but
    // part of the documented flag set), an incomplete frame cut short by a
    // new opcode, a checksum (XOR) mismatch, or a stray data byte received
    // while not mid-frame. msgEcho (0x10) is deliberately excluded: it marks
    // a legitimate echo of our own transmission, not invalid data.
    constexpr uint8_t kInvalidFrameFlags =
        errorCollision | errorFrame | errorTimeout | errorCarrierLoss | msgIncomplete | msgXORCheck | msgStrayData;

    void onLocoNetMessageReceived(lnReceiveBuffer* buffer)
    {
        if ((buffer->errorFlags & kInvalidFrameFlags) != 0)
        {
            return;
        }
        const size_t size = std::min<size_t>(buffer->lnMsgSize, lnMaxMsgSize);
        if (pendingMessages().size() >= kMaxPendingMessages)
        {
            pendingMessages().pop();
        }
        pendingMessages().emplace(buffer->lnData, buffer->lnData + size);
    }
}

LocoNetEsp32Port::LocoNetEsp32Port(int rxPin, int txPin)
    : serial_(rxPin, txPin, /* inverse_logic = */ true)
{
    serial_.setLNCallback(&onLocoNetMessageReceived);
}

void LocoNetEsp32Port::begin()
{
    // No-op: LocoNetESPSerial's constructor already calls its own begin()
    // internally whenever both pins are non-negative (confirmed in
    // IoTT_LocoNetHBESP32.cpp), so calling serial_.begin() again here would
    // re-run hardware init a second time (duplicate timer setup, duplicate
    // UART begin, a leaked interrupt handle). Kept as a method — rather than
    // removed from the class — so the adapter's public shape (begin()/
    // update() lifecycle, matching other adapters) stays stable even though
    // there's currently nothing left for it to do.
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
    const size_t size = std::min<size_t>(message.bytes().size(), lnMaxMsgSize);
    packet.lnMsgSize = static_cast<uint8_t>(size);
    std::copy_n(message.bytes().begin(), size, packet.lnData);
    serial_.lnWriteMsg(packet);
}

#endif
