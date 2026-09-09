#include "FlashingMessageLog.h"

FlashingMessageLog::FlashingMessageLog(MessageLog& inner, ActivityIndicator& indicator)
    : inner_(inner), indicator_(indicator)
{
}

void FlashingMessageLog::record(const LocoNetMessage& message)
{
    indicator_.flash();
    inner_.record(message);
}
