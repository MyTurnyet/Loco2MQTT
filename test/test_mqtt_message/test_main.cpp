#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/IncomingMqttMessage.h"
#include "domain/MqttMessage.h"

TEST_CASE("MqttMessage stores topic, payload, and retained")
{
    MqttMessage message("loconet/turnout/5/state", "CLOSED", true);

    REQUIRE(message.topic() == "loconet/turnout/5/state");
    REQUIRE(message.payload() == "CLOSED");
    REQUIRE(message.retained() == true);
}

TEST_CASE("IncomingMqttMessage stores topic and payload")
{
    IncomingMqttMessage message("loconet/turnout/5/set", "THROWN");

    REQUIRE(message.topic() == "loconet/turnout/5/set");
    REQUIRE(message.payload() == "THROWN");
}
