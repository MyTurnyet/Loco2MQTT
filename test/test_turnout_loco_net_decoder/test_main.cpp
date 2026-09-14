#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/TurnoutStateChanged.h"
#include "turnout/PendingTurnoutStateAcks.h"
#include "turnout/TurnoutLocoNetDecoder.h"

TEST_CASE("canDecode is true for OPC_SW_REQ, OPC_SW_REP, and OPC_LONG_ACK, false otherwise")
{
    PendingTurnoutStateAcks pendingAcks;
    TurnoutLocoNetDecoder decoder(pendingAcks);

    REQUIRE(decoder.canDecode(0xB0) == true);
    REQUIRE(decoder.canDecode(0xB1) == true);
    REQUIRE(decoder.canDecode(0xB4) == true);
    REQUIRE(decoder.canDecode(0x81) == false);
}

TEST_CASE("decodes an OPC_SW_REQ on-pulse for address 5, Closed")
{
    PendingTurnoutStateAcks pendingAcks;
    TurnoutLocoNetDecoder decoder(pendingAcks);

    auto event = decoder.decode(LocoNetMessage({0xB0, 0x04, 0x30, 0x7B}));

    REQUIRE(event.has_value());
    auto& state = std::get<TurnoutStateChanged>(*event);
    REQUIRE(state.address().value() == 5);
    REQUIRE(state.position() == TurnoutPosition::Closed);
}

TEST_CASE("decodes an OPC_SW_REQ for address 5, Thrown")
{
    PendingTurnoutStateAcks pendingAcks;
    TurnoutLocoNetDecoder decoder(pendingAcks);

    auto event = decoder.decode(LocoNetMessage({0xB0, 0x04, 0x10, 0x5B}));

    REQUIRE(event.has_value());
    REQUIRE(std::get<TurnoutStateChanged>(*event).position() == TurnoutPosition::Thrown);
}

TEST_CASE("decodes the maximum address, 2048")
{
    PendingTurnoutStateAcks pendingAcks;
    TurnoutLocoNetDecoder decoder(pendingAcks);

    auto event = decoder.decode(LocoNetMessage({0xB0, 0x7F, 0x3F, 0x0F}));

    REQUIRE(event.has_value());
    REQUIRE(std::get<TurnoutStateChanged>(*event).address().value() == 2048);
}

TEST_CASE("the off-pulse following an on-pulse is deduped as no change")
{
    PendingTurnoutStateAcks pendingAcks;
    TurnoutLocoNetDecoder decoder(pendingAcks);
    decoder.decode(LocoNetMessage({0xB0, 0x04, 0x30, 0x7B}));

    auto event = decoder.decode(LocoNetMessage({0xB0, 0x04, 0x20, 0x6B}));

    REQUIRE(event.has_value() == false);
}

TEST_CASE("a genuine position change after a known state is not deduped")
{
    PendingTurnoutStateAcks pendingAcks;
    TurnoutLocoNetDecoder decoder(pendingAcks);
    decoder.decode(LocoNetMessage({0xB0, 0x04, 0x30, 0x7B}));

    auto event = decoder.decode(LocoNetMessage({0xB0, 0x04, 0x10, 0x5B}));

    REQUIRE(event.has_value());
    REQUIRE(std::get<TurnoutStateChanged>(*event).position() == TurnoutPosition::Thrown);
}

TEST_CASE("decodes an OPC_SW_REP switch report as a new state")
{
    PendingTurnoutStateAcks pendingAcks;
    TurnoutLocoNetDecoder decoder(pendingAcks);

    auto event = decoder.decode(LocoNetMessage({0xB1, 0x04, 0x20, 0x6A}));

    REQUIRE(event.has_value());
    auto& state = std::get<TurnoutStateChanged>(*event);
    REQUIRE(state.address().value() == 5);
    REQUIRE(state.position() == TurnoutPosition::Closed);
}

TEST_CASE("an OPC_SW_REP sensor report is not a commanded-position event")
{
    PendingTurnoutStateAcks pendingAcks;
    TurnoutLocoNetDecoder decoder(pendingAcks);

    auto event = decoder.decode(LocoNetMessage({0xB1, 0x04, 0x40, 0x0A}));

    REQUIRE(event.has_value() == false);
}

TEST_CASE("allKnownStates returns every address seen so far")
{
    PendingTurnoutStateAcks pendingAcks;
    TurnoutLocoNetDecoder decoder(pendingAcks);
    decoder.decode(LocoNetMessage({0xB0, 0x04, 0x30, 0x7B}));
    decoder.decode(LocoNetMessage({0xB0, 0x06, 0x10, 0x59}));

    auto states = decoder.allKnownStates();

    REQUIRE(states.size() == 2);
    REQUIRE(std::get<TurnoutStateChanged>(states[0]).address().value() == 5);
    REQUIRE(std::get<TurnoutStateChanged>(states[1]).address().value() == 7);
}

TEST_CASE("allKnownStates is empty before anything has been decoded")
{
    PendingTurnoutStateAcks pendingAcks;
    TurnoutLocoNetDecoder decoder(pendingAcks);

    REQUIRE(decoder.allKnownStates().empty());
}

// --- OPC_LONG_ACK (0xB4) switch-state-ack handling ---
//
// OPC_SW_STATE (0xBC) is *not* answered with OPC_SW_REP, as originally
// assumed -- a real capture against Paige's DR5000 (2026-09-14, see ADR
// 0001's addendum) showed [BC 00 00 43] "Request status of switch LT1"
// answered by [B4 3C 30 47] "LONG_ACK: ... switch state request 0x30
// (Closed)". OPC_LONG_ACK carries no address, so PendingTurnoutStateAcks
// supplies it from send order.

TEST_CASE("decodes an OPC_LONG_ACK switch-state ack as the oldest pending address, Closed")
{
    PendingTurnoutStateAcks pendingAcks;
    pendingAcks.expect(TurnoutAddress(1));
    TurnoutLocoNetDecoder decoder(pendingAcks);

    auto event = decoder.decode(LocoNetMessage({0xB4, 0x3C, 0x30, 0x47}));

    REQUIRE(event.has_value());
    auto& state = std::get<TurnoutStateChanged>(*event);
    REQUIRE(state.address().value() == 1);
    REQUIRE(state.position() == TurnoutPosition::Closed);
}

TEST_CASE("decodes an OPC_LONG_ACK switch-state ack as the oldest pending address, Thrown")
{
    PendingTurnoutStateAcks pendingAcks;
    pendingAcks.expect(TurnoutAddress(5));
    TurnoutLocoNetDecoder decoder(pendingAcks);

    auto event = decoder.decode(LocoNetMessage({0xB4, 0x3C, 0x00, 0x77}));

    REQUIRE(event.has_value());
    auto& state = std::get<TurnoutStateChanged>(*event);
    REQUIRE(state.address().value() == 5);
    REQUIRE(state.position() == TurnoutPosition::Thrown);
}

TEST_CASE("claims pending addresses in FIFO order across successive acks")
{
    PendingTurnoutStateAcks pendingAcks;
    pendingAcks.expect(TurnoutAddress(1));
    pendingAcks.expect(TurnoutAddress(2));
    TurnoutLocoNetDecoder decoder(pendingAcks);

    auto first = decoder.decode(LocoNetMessage({0xB4, 0x3C, 0x30, 0x47}));
    auto second = decoder.decode(LocoNetMessage({0xB4, 0x3C, 0x00, 0x77}));

    REQUIRE(std::get<TurnoutStateChanged>(*first).address().value() == 1);
    REQUIRE(std::get<TurnoutStateChanged>(*second).address().value() == 2);
}

TEST_CASE("ignores an OPC_LONG_ACK when nothing is pending")
{
    PendingTurnoutStateAcks pendingAcks;
    TurnoutLocoNetDecoder decoder(pendingAcks);

    auto event = decoder.decode(LocoNetMessage({0xB4, 0x3C, 0x30, 0x47}));

    REQUIRE(event.has_value() == false);
}

TEST_CASE("ignores an OPC_LONG_ACK for an unrelated command (not a switch-state ack)")
{
    PendingTurnoutStateAcks pendingAcks;
    pendingAcks.expect(TurnoutAddress(1));
    TurnoutLocoNetDecoder decoder(pendingAcks);

    auto event = decoder.decode(LocoNetMessage({0xB4, 0x7F, 0x00, 0x34}));

    REQUIRE(event.has_value() == false);
}

TEST_CASE("a state ack matching the already-known position is deduped")
{
    PendingTurnoutStateAcks pendingAcks;
    TurnoutLocoNetDecoder decoder(pendingAcks);
    decoder.decode(LocoNetMessage({0xB0, 0x00, 0x30, 0x7F}));
    pendingAcks.expect(TurnoutAddress(1));

    auto event = decoder.decode(LocoNetMessage({0xB4, 0x3C, 0x30, 0x47}));

    REQUIRE(event.has_value() == false);
}
