#pragma once

#include "ports/ActivityIndicator.h"
#include "ports/MessageLog.h"

// Decorates another MessageLog so every recorded message also triggers an
// ActivityIndicator. LocoNetPort::receive() may have only one drain per
// boot mode (see CLAUDE.md's note on the destructive-read queue), and
// every boot mode already routes each received message through exactly
// one MessageLog::record() call -- wrapping that call, rather than adding
// a second receive() consumer, is how LocoNet activity reaches a second
// observer.
class FlashingMessageLog final : public MessageLog
{
public:
    FlashingMessageLog(MessageLog& inner, ActivityIndicator& indicator);

    void record(const LocoNetMessage& message) override;

private:
    MessageLog& inner_;
    ActivityIndicator& indicator_;
};
