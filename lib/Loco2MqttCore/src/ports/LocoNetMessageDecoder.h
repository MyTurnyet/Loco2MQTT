#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "domain/DomainEvent.h"
#include "domain/LocoNetMessage.h"

class LocoNetMessageDecoder
{
public:
    virtual ~LocoNetMessageDecoder() = default;
    virtual bool canDecode(uint8_t opcode) const = 0;
    virtual std::optional<DomainEvent> decode(const LocoNetMessage& message) = 0;
    virtual std::vector<DomainEvent> allKnownStates() const = 0;
};
