#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/SetTurnoutPosition.h"
#include "turnout/TurnoutMqttCommandDecoder.h"

TEST_CASE("canDecode is true only for the turnout device type")
{
    TurnoutMqttCommandDecoder decoder;

    REQUIRE(decoder.canDecode("turnout") == true);
    REQUIRE(decoder.canDecode("sensor") == false);
}

TEST_CASE("decodes a valid CLOSED command")
{
    TurnoutMqttCommandDecoder decoder;

    auto command = decoder.decode("5", "CLOSED");

    REQUIRE(command.has_value());
    auto& set = std::get<SetTurnoutPosition>(*command);
    REQUIRE(set.address().value() == 5);
    REQUIRE(set.position() == TurnoutPosition::Closed);
}

TEST_CASE("decodes a valid THROWN command")
{
    TurnoutMqttCommandDecoder decoder;

    auto command = decoder.decode("2048", "THROWN");

    REQUIRE(command.has_value());
    auto& set = std::get<SetTurnoutPosition>(*command);
    REQUIRE(set.address().value() == 2048);
    REQUIRE(set.position() == TurnoutPosition::Thrown);
}

TEST_CASE("rejects a non-numeric address")
{
    TurnoutMqttCommandDecoder decoder;

    REQUIRE(decoder.decode("abc", "CLOSED").has_value() == false);
}

TEST_CASE("rejects an address below the valid range")
{
    TurnoutMqttCommandDecoder decoder;

    REQUIRE(decoder.decode("0", "CLOSED").has_value() == false);
}

TEST_CASE("rejects an address above the valid range")
{
    TurnoutMqttCommandDecoder decoder;

    REQUIRE(decoder.decode("2049", "CLOSED").has_value() == false);
}

TEST_CASE("rejects a payload that is not exactly CLOSED or THROWN")
{
    TurnoutMqttCommandDecoder decoder;

    REQUIRE(decoder.decode("5", "closed").has_value() == false);
    REQUIRE(decoder.decode("5", "SIDEWAYS").has_value() == false);
}
