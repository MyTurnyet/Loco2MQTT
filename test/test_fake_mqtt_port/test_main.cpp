#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "support/FakeMqttPort.h"

TEST_CASE("receiveCommand returns nullopt when nothing is queued")
{
    FakeMqttPort port;

    REQUIRE(port.receiveCommand().has_value() == false);
}

TEST_CASE("receiveCommand returns an enqueued command, then nullopt again")
{
    FakeMqttPort port;
    port.enqueueCommand(IncomingMqttMessage("loconet/turnout/5/set", "CLOSED"));

    auto received = port.receiveCommand();

    REQUIRE(received.has_value());
    REQUIRE(received->topic() == "loconet/turnout/5/set");
    REQUIRE(port.receiveCommand().has_value() == false);
}

TEST_CASE("enqueued commands are read back in FIFO order")
{
    FakeMqttPort port;
    port.enqueueCommand(IncomingMqttMessage("a", "1"));
    port.enqueueCommand(IncomingMqttMessage("b", "2"));

    REQUIRE(port.receiveCommand()->topic() == "a");
    REQUIRE(port.receiveCommand()->topic() == "b");
}

TEST_CASE("publish records published messages in order")
{
    FakeMqttPort port;
    port.publish(MqttMessage("loconet/turnout/5/state", "CLOSED", true));
    port.publish(MqttMessage("loconet/turnout/7/state", "THROWN", true));

    REQUIRE(port.published().size() == 2);
    REQUIRE(port.published()[0].topic() == "loconet/turnout/5/state");
    REQUIRE(port.published()[1].topic() == "loconet/turnout/7/state");
}
