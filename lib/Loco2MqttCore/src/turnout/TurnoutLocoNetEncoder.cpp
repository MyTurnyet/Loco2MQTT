#include "turnout/TurnoutLocoNetEncoder.h"

#include "domain/LocoNetChecksum.h"
#include "domain/SetTurnoutPosition.h"

namespace
{
    constexpr uint8_t kOpcSwReq = 0xB0;
    constexpr uint8_t kClosedBit = 0x20;
    constexpr uint8_t kOutputOnBit = 0x10;

    LocoNetMessage buildSwReq(const SetTurnoutPosition& command, bool outputOn)
    {
        const int zeroBasedAddress = command.address().value() - 1;
        const uint8_t sw1 = zeroBasedAddress & 0x7F;
        uint8_t sw2 = (zeroBasedAddress >> 7) & 0x0F;
        sw2 |= command.position() == TurnoutPosition::Closed ? kClosedBit : 0x00;
        sw2 |= outputOn ? kOutputOnBit : 0x00;
        const uint8_t checksum = computeLocoNetChecksum({kOpcSwReq, sw1, sw2});
        return LocoNetMessage({kOpcSwReq, sw1, sw2, checksum});
    }
}

void TurnoutLocoNetEncoder::encode(const DomainCommand& command, LocoNetSendScheduler& scheduler) const
{
    const auto& setPosition = std::get<SetTurnoutPosition>(command);
    scheduler.sendNow(buildSwReq(setPosition, /* outputOn = */ true));
    scheduler.sendAfter(buildSwReq(setPosition, /* outputOn = */ false), kOffPulseDelayMs);
}
