#include "LocoNetMessageLogger.h"

void LocoNetMessageLogger::update()
{
    std::optional<LocoNetMessage> message = port_.receive();
    while (message.has_value())
    {
        log_.record(*message);
        message = port_.receive();
    }
}
