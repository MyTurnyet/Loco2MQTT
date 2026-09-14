#include "turnout/TurnoutLocoNetDecoder.h"

namespace
{
    constexpr uint8_t kOpcSwReq = 0xB0;
    constexpr uint8_t kOpcSwRep = 0xB1;
    constexpr uint8_t kOpcLongAck = 0xB4;
    constexpr uint8_t kSwRepInputs = 0x40;
    constexpr uint8_t kClosedBit = 0x20;

    // OPC_LONG_ACK's D1 byte identifying "this acks an OPC_SW_REQ/
    // OPC_SW_STATE switch command" -- confirmed against a real capture of
    // this command station's own replies (2026-09-14, see ADR 0001's
    // addendum): every OPC_SW_STATE reply observed carried D1 == 0x3C.
    constexpr uint8_t kSwitchStateAckCode = 0x3C;

    int addressFrom(const LocoNetMessage& message)
    {
        return (((message.bytes()[2] & 0x0F) << 7) | (message.bytes()[1] & 0x7F)) + 1;
    }

    // Also used to read OPC_LONG_ACK's D2 byte -- same kClosedBit
    // convention, confirmed by the same capture (D2 0x30 -> Closed,
    // 0x00 -> Thrown).
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
    return opcode == kOpcSwReq || opcode == kOpcSwRep || opcode == kOpcLongAck;
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
    return message.bytes()[0] == kOpcLongAck ? decodeStateAck(message) : decodeSwitchMessage(message);
}

std::optional<DomainEvent> TurnoutLocoNetDecoder::decodeSwitchMessage(const LocoNetMessage& message)
{
    if (isSensorReport(message))
        return std::nullopt;

    return emitIfChanged(addressFrom(message), positionFrom(message));
}

// OPC_LONG_ACK carries no address -- PendingTurnoutStateAcks supplies the
// one this ack must belong to, on the assumption (see that class's own
// comment) that the command station answers OPC_SW_STATE requests one at
// a time, in send order.
std::optional<DomainEvent> TurnoutLocoNetDecoder::decodeStateAck(const LocoNetMessage& message)
{
    if (message.bytes()[1] != kSwitchStateAckCode)
        return std::nullopt;

    std::optional<TurnoutAddress> address = pendingAcks_.claimNext();
    if (!address.has_value())
        return std::nullopt;

    return emitIfChanged(address->value(), positionFrom(message));
}

std::optional<DomainEvent> TurnoutLocoNetDecoder::emitIfChanged(int address, TurnoutPosition position)
{
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
