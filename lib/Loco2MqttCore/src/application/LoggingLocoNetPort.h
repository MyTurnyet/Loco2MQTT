#pragma once

#include "ports/LocoNetPort.h"
#include "ports/MessageLog.h"

// Decorates a LocoNetPort so every send() is also recorded to a MessageLog.
// receive() is passed straight through and not logged here: the one
// receive() consumer per boot mode already records what it drains (see
// CLAUDE.md's note on the destructive-read queue), so logging it again
// would duplicate every received message.
class LoggingLocoNetPort final : public LocoNetPort
{
public:
    LoggingLocoNetPort(LocoNetPort& inner, MessageLog& sentLog) : inner_(inner), sentLog_(sentLog)
    {
    }

    std::optional<LocoNetMessage> receive() override
    {
        return inner_.receive();
    }

    void send(const LocoNetMessage& message) override
    {
        sentLog_.record(message);
        inner_.send(message);
    }

private:
    LocoNetPort& inner_;
    MessageLog& sentLog_;
};
