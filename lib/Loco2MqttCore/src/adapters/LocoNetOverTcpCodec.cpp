#include "LocoNetOverTcpCodec.h"

std::string LocoNetOverTcpCodec::encodeSend(const LocoNetMessage& message) const
{
    return "SEND " + message.describe();
}
