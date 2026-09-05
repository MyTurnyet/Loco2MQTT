#include "LocoNetMessageLogger.h"

void LocoNetMessageLogger::update()
{
    std::optional<LocoNetMessage> message = port_.receive();
    if (message.has_value())
    {
        log_.record(*message);
    }
}
