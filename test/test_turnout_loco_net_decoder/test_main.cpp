#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/TurnoutStateChanged.h"
#include "turnout/TurnoutLocoNetDecoder.h"

TEST_CASE("canDecode is true for OPC_SW_REQ and OPC_SW_REP, false otherwise")
{
    TurnoutLocoNetDecoder decoder;

    REQUIRE(decoder.canDecode(0xB0) == true);
    REQUIRE(decoder.canDecode(0xB1) == true);
    REQUIRE(decoder.canDecode(0x81) == false);
}

TEST_CASE("decodes an OPC_SW_REQ on-pulse for address 5, Closed")
{
    TurnoutLocoNetDecoder decoder;

    auto event = decoder.decode(LocoNetMessage({0xB0, 0x04, 0x30, 0x7B}));

    REQUIRE(event.has_value());
    auto& state = std::get<TurnoutStateChanged>(*event);
    REQUIRE(state.address().value() == 5);
    REQUIRE(state.position() == TurnoutPosition::Closed);
}

TEST_CASE("decodes an OPC_SW_REQ for address 5, Thrown")
{
    TurnoutLocoNetDecoder decoder;

    auto event = decoder.decode(LocoNetMessage({0xB0, 0x04, 0x10, 0x5B}));

    REQUIRE(event.has_value());
    REQUIRE(std::get<TurnoutStateChanged>(*event).position() == TurnoutPosition::Thrown);
}

TEST_CASE("decodes the maximum address, 2048")
{
    TurnoutLocoNetDecoder decoder;

    auto event = decoder.decode(LocoNetMessage({0xB0, 0x7F, 0x3F, 0x0F}));

    REQUIRE(event.has_value());
    REQUIRE(std::get<TurnoutStateChanged>(*event).address().value() == 2048);
}

TEST_CASE("the off-pulse following an on-pulse is deduped as no change")
{
    TurnoutLocoNetDecoder decoder;
    decoder.decode(LocoNetMessage({0xB0, 0x04, 0x30, 0x7B}));

    auto event = decoder.decode(LocoNetMessage({0xB0, 0x04, 0x20, 0x6B}));

    REQUIRE(event.has_value() == false);
}

TEST_CASE("a genuine position change after a known state is not deduped")
{
    TurnoutLocoNetDecoder decoder;
    decoder.decode(LocoNetMessage({0xB0, 0x04, 0x30, 0x7B}));

    auto event = decoder.decode(LocoNetMessage({0xB0, 0x04, 0x10, 0x5B}));

    REQUIRE(event.has_value());
    REQUIRE(std::get<TurnoutStateChanged>(*event).position() == TurnoutPosition::Thrown);
}

TEST_CASE("decodes an OPC_SW_REP switch report as a new state")
{
    TurnoutLocoNetDecoder decoder;

    auto event = decoder.decode(LocoNetMessage({0xB1, 0x04, 0x20, 0x6A}));

    REQUIRE(event.has_value());
    auto& state = std::get<TurnoutStateChanged>(*event);
    REQUIRE(state.address().value() == 5);
    REQUIRE(state.position() == TurnoutPosition::Closed);
}

TEST_CASE("an OPC_SW_REP sensor report is not a commanded-position event")
{
    TurnoutLocoNetDecoder decoder;

    auto event = decoder.decode(LocoNetMessage({0xB1, 0x04, 0x40, 0x0A}));

    REQUIRE(event.has_value() == false);
}

TEST_CASE("allKnownStates returns every address seen so far")
{
    TurnoutLocoNetDecoder decoder;
    decoder.decode(LocoNetMessage({0xB0, 0x04, 0x30, 0x7B}));
    decoder.decode(LocoNetMessage({0xB0, 0x06, 0x10, 0x59}));

    auto states = decoder.allKnownStates();

    REQUIRE(states.size() == 2);
    REQUIRE(std::get<TurnoutStateChanged>(states[0]).address().value() == 5);
    REQUIRE(std::get<TurnoutStateChanged>(states[1]).address().value() == 7);
}

TEST_CASE("allKnownStates is empty before anything has been decoded")
{
    TurnoutLocoNetDecoder decoder;

    REQUIRE(decoder.allKnownStates().empty());
}
