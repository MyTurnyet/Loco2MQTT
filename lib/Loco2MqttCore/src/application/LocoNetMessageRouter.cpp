#include "application/LocoNetMessageRouter.h"

void LocoNetMessageRouter::update()
{
    std::optional<LocoNetMessage> message = locoNetPort_.receive();
    while (message.has_value())
    {
        handleMessage(*message);
        message = locoNetPort_.receive();
    }
    republishAllKnownStatesIfDue();
}

void LocoNetMessageRouter::handleMessage(const LocoNetMessage& message)
{
    log_.record(message);
    // OPC_SW_REQ/OPC_SW_REP (and every other opcode this project decodes)
    // are fixed 4-byte frames. The vendor library validates checksums on
    // receipt but does not enforce a lower length bound on lnMsgSize (see
    // CLAUDE.md's note on the long-form-frame length byte), so a short
    // frame is guarded here, once, rather than in every decoder.
    if (message.bytes().size() < 4)
    {
        return;
    }
    for (auto& [decoder, encoder] : decoders_)
    {
        if (!decoder->canDecode(message.bytes()[0]))
        {
            continue;
        }
        std::optional<DomainEvent> event = decoder->decode(message);
        if (event.has_value())
        {
            mqttPort_.publish(encoder->encode(*event));
        }
        return;
    }
}

void LocoNetMessageRouter::republishAllKnownStatesIfDue()
{
    if (clock_.nowMilliseconds() - lastRepublishAtMilliseconds_ < kStateRepublishIntervalMs)
    {
        return;
    }
    lastRepublishAtMilliseconds_ = clock_.nowMilliseconds();
    for (auto& [decoder, encoder] : decoders_)
    {
        for (const DomainEvent& event : decoder->allKnownStates())
        {
            mqttPort_.publish(encoder->encode(event));
        }
    }
}
