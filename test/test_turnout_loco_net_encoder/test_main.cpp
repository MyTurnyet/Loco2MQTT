#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/SetTurnoutPosition.h"
#include "domain/TurnoutAddress.h"
#include "support/FakeLocoNetSendScheduler.h"
#include "turnout/TurnoutLocoNetEncoder.h"

TEST_CASE("sends the on-pulse immediately for a Closed command")
{
    TurnoutLocoNetEncoder encoder;
    FakeLocoNetSendScheduler scheduler;

    encoder.encode(DomainCommand(SetTurnoutPosition(TurnoutAddress(5), TurnoutPosition::Closed)), scheduler);

    REQUIRE(scheduler.sentNow().size() == 1);
    REQUIRE(scheduler.sentNow()[0].bytes() == std::vector<uint8_t>{0xB0, 0x04, 0x30, 0x7B});
}

TEST_CASE("schedules the off-pulse 250ms later, same address and direction, output off")
{
    TurnoutLocoNetEncoder encoder;
    FakeLocoNetSendScheduler scheduler;

    encoder.encode(DomainCommand(SetTurnoutPosition(TurnoutAddress(5), TurnoutPosition::Closed)), scheduler);

    REQUIRE(scheduler.scheduled().size() == 1);
    REQUIRE(scheduler.scheduled()[0].first.bytes() == std::vector<uint8_t>{0xB0, 0x04, 0x20, 0x6B});
    REQUIRE(scheduler.scheduled()[0].second == 250);
}

TEST_CASE("encodes a Thrown command with the direction bit clear")
{
    TurnoutLocoNetEncoder encoder;
    FakeLocoNetSendScheduler scheduler;

    encoder.encode(DomainCommand(SetTurnoutPosition(TurnoutAddress(5), TurnoutPosition::Thrown)), scheduler);

    REQUIRE(scheduler.sentNow()[0].bytes() == std::vector<uint8_t>{0xB0, 0x04, 0x10, 0x5B});
    REQUIRE(scheduler.scheduled()[0].first.bytes() == std::vector<uint8_t>{0xB0, 0x04, 0x00, 0x4B});
}

TEST_CASE("encodes the maximum address, 2048")
{
    TurnoutLocoNetEncoder encoder;
    FakeLocoNetSendScheduler scheduler;

    encoder.encode(DomainCommand(SetTurnoutPosition(TurnoutAddress(2048), TurnoutPosition::Closed)), scheduler);

    REQUIRE(scheduler.sentNow()[0].bytes() == std::vector<uint8_t>{0xB0, 0x7F, 0x3F, 0x0F});
}
