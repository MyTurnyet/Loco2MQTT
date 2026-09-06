#include "turnout/TurnoutLocoNetDecoder.h"

namespace
{
    constexpr uint8_t kOpcSwReq = 0xB0;
    constexpr uint8_t kOpcSwRep = 0xB1;
    constexpr uint8_t kSwRepInputs = 0x40;
    constexpr uint8_t kClosedBit = 0x20;

    int addressFrom(const LocoNetMessage& message)
    {
        return (((message.bytes()[2] & 0x0F) << 7) | (message.bytes()[1] & 0x7F)) + 1;
    }

    TurnoutPosition positionFrom(const LocoNetMessage& message)
    {
        return (message.bytes()[2] & kClosedBit) ? TurnoutPosition::Closed : TurnoutPosition::Thrown;
    }

    bool isSensorReport(const LocoNetMessage& message)
    {
        return message.bytes()[0] == kOpcSwRep && (message.bytes()[2] & kSwRepInputs);
    }
}

bool TurnoutLocoNetDecoder::canDecode(uint8_t opcode) const
{
    return opcode == kOpcSwReq || opcode == kOpcSwRep;
}

bool TurnoutLocoNetDecoder::shouldEmitPositionChange(int address, TurnoutPosition position)
{
    auto existing = lastKnownPosition_.find(address);
    if (existing != lastKnownPosition_.end() && existing->second == position)
    {
        return false;
    }
    lastKnownPosition_[address] = position;
    return true;
}

std::optional<DomainEvent> TurnoutLocoNetDecoder::decode(const LocoNetMessage& message)
{
    if (isSensorReport(message))
        return std::nullopt;

    const int address = addressFrom(message);
    const TurnoutPosition position = positionFrom(message);

    if (!shouldEmitPositionChange(address, position))
        return std::nullopt;

    return DomainEvent(TurnoutStateChanged(TurnoutAddress(address), position));
}

std::vector<DomainEvent> TurnoutLocoNetDecoder::allKnownStates() const
{
    std::vector<DomainEvent> states;
    for (const auto& [address, position] : lastKnownPosition_)
    {
        states.emplace_back(TurnoutStateChanged(TurnoutAddress(address), position));
    }
    return states;
}
