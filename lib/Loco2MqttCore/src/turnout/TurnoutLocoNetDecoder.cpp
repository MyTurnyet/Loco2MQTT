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
}

bool TurnoutLocoNetDecoder::canDecode(uint8_t opcode) const
{
    return opcode == kOpcSwReq || opcode == kOpcSwRep;
}

std::optional<DomainEvent> TurnoutLocoNetDecoder::decode(const LocoNetMessage& message)
{
    if (message.bytes()[0] == kOpcSwRep && (message.bytes()[2] & kSwRepInputs))
    {
        return std::nullopt;
    }
    const int address = addressFrom(message);
    const TurnoutPosition position = positionFrom(message);
    auto existing = lastKnownPosition_.find(address);
    if (existing != lastKnownPosition_.end() && existing->second == position)
    {
        return std::nullopt;
    }
    lastKnownPosition_[address] = position;
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
