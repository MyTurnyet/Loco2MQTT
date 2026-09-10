#include "LocoNetOverTcpPort.h"

std::optional<LocoNetMessage> LocoNetOverTcpPort::receive()
{
    std::optional<std::string> line = stream_.readLine();
    return line.has_value() ? decodeNextMessage(*line) : std::nullopt;
}

void LocoNetOverTcpPort::send(const LocoNetMessage& message)
{
    stream_.writeLine(codec_.encodeSend(message));
}

// RECEIVE/SEND aren't the only lines JMRI's server sends (VERSION on
// connect, SENT OK/ERROR after every SEND). Those decode to nullopt, but
// a real message may still be queued right behind one — so this skips
// non-data lines transparently, one at a time, rather than surfacing a
// spurious "no message" to the caller. Bounded by however many non-data
// lines are buffered ahead of the next real one (a handful, in practice).
std::optional<LocoNetMessage> LocoNetOverTcpPort::decodeNextMessage(const std::string& line)
{
    std::optional<LocoNetMessage> message = codec_.decodeLine(line);
    return message.has_value() ? message : receive();
}
