#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/TurnoutAddress.h"
#include "domain/TurnoutStateChanged.h"
#include "turnout/TurnoutMqttEncoder.h"

TEST_CASE("encodes a Closed event to the state topic")
{
    TurnoutMqttEncoder encoder;

    MqttMessage message = encoder.encode(DomainEvent(TurnoutStateChanged(TurnoutAddress(5), TurnoutPosition::Closed)));

    REQUIRE(message.topic() == "loconet/turnout/5/state");
    REQUIRE(message.payload() == "CLOSED");
    REQUIRE(message.retained() == true);
}

TEST_CASE("encodes a Thrown event with the correct payload")
{
    TurnoutMqttEncoder encoder;

    MqttMessage message = encoder.encode(DomainEvent(TurnoutStateChanged(TurnoutAddress(7), TurnoutPosition::Thrown)));

    REQUIRE(message.topic() == "loconet/turnout/7/state");
    REQUIRE(message.payload() == "THROWN");
}
