#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/TurnoutAddress.h"
#include "turnout/PendingTurnoutStateAcks.h"

TEST_CASE("claimNext returns nullopt when nothing is expected")
{
    PendingTurnoutStateAcks pending;

    REQUIRE(pending.claimNext().has_value() == false);
}

TEST_CASE("claimNext returns the one expected address")
{
    PendingTurnoutStateAcks pending;
    pending.expect(TurnoutAddress(5));

    auto claimed = pending.claimNext();

    REQUIRE(claimed.has_value());
    REQUIRE(claimed->value() == 5);
}

TEST_CASE("claims addresses in FIFO order, oldest expectation first")
{
    PendingTurnoutStateAcks pending;
    pending.expect(TurnoutAddress(1));
    pending.expect(TurnoutAddress(2));
    pending.expect(TurnoutAddress(3));

    REQUIRE(pending.claimNext()->value() == 1);
    REQUIRE(pending.claimNext()->value() == 2);
    REQUIRE(pending.claimNext()->value() == 3);
}

TEST_CASE("each claim consumes exactly one expectation")
{
    PendingTurnoutStateAcks pending;
    pending.expect(TurnoutAddress(1));
    pending.expect(TurnoutAddress(2));

    pending.claimNext();

    REQUIRE(pending.claimNext()->value() == 2);
    REQUIRE(pending.claimNext().has_value() == false);
}
