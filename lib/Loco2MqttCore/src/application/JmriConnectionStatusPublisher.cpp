#include "application/JmriConnectionStatusPublisher.h"

namespace
{
    constexpr const char* kTopic = "loco2mqtt/jmri/status";
}

JmriConnectionStatusPublisher::JmriConnectionStatusPublisher(LineStream& stream, MqttPort& mqttPort, Clock& clock)
    : stream_(stream), mqttPort_(mqttPort), clock_(clock)
{
}

void JmriConnectionStatusPublisher::update()
{
    publishIfChanged();
    republishIfDue();
}

void JmriConnectionStatusPublisher::publishIfChanged()
{
    const bool connected = stream_.isConnected();
    if (lastKnownConnected_.has_value() && *lastKnownConnected_ == connected)
    {
        return;
    }
    lastKnownConnected_ = connected;
    publish();
}

void JmriConnectionStatusPublisher::republishIfDue()
{
    if (clock_.nowMilliseconds() - lastPublishedAtMs_ < kRepublishIntervalMs)
    {
        return;
    }
    publish();
}

void JmriConnectionStatusPublisher::publish()
{
    const std::string payload = stream_.isConnected() ? "connected" : "disconnected";
    mqttPort_.publish(MqttMessage(kTopic, payload, true));
    lastPublishedAtMs_ = clock_.nowMilliseconds();
}