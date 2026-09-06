#include "turnout/TurnoutMqttEncoder.h"

MqttMessage TurnoutMqttEncoder::encode(const DomainEvent& event) const
{
    const auto& state = std::get<TurnoutStateChanged>(event);
    const std::string topic = "loconet/turnout/" + std::to_string(state.address().value()) + "/state";
    const std::string payload = state.position() == TurnoutPosition::Closed ? "CLOSED" : "THROWN";
    return MqttMessage(topic, payload, true);
}
