#include "application/PendingLocoNetSendScheduler.h"

void PendingLocoNetSendScheduler::sendNow(const LocoNetMessage& message)
{
    port_.send(message);
}

void PendingLocoNetSendScheduler::sendAfter(const LocoNetMessage& message, unsigned long delayMilliseconds)
{
    pending_.emplace_back(message, clock_.nowMilliseconds() + delayMilliseconds);
}

void PendingLocoNetSendScheduler::update()
{
    const unsigned long now = clock_.nowMilliseconds();
    auto it = pending_.begin();
    while (it != pending_.end())
    {
        if (it->dueAtMilliseconds() > now)
        {
            ++it;
            continue;
        }
        port_.send(it->message());
        it = pending_.erase(it);
    }
}
