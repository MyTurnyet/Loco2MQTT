#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include <variant>

#include "domain/DomainCommand.h"
#include "domain/DomainEvent.h"
#include "domain/SetTurnoutPosition.h"
#include "domain/TurnoutAddress.h"
#include "domain/TurnoutPosition.h"
#include "domain/TurnoutStateChanged.h"

TEST_CASE("TurnoutStateChanged stores address and position")
{
    TurnoutStateChanged event(TurnoutAddress(5), TurnoutPosition::Closed);

    REQUIRE(event.address().value() == 5);
    REQUIRE(event.position() == TurnoutPosition::Closed);
}

TEST_CASE("SetTurnoutPosition stores address and position")
{
    SetTurnoutPosition command(TurnoutAddress(7), TurnoutPosition::Thrown);

    REQUIRE(command.address().value() == 7);
    REQUIRE(command.position() == TurnoutPosition::Thrown);
}

TEST_CASE("DomainEvent holds a TurnoutStateChanged")
{
    DomainEvent event = TurnoutStateChanged(TurnoutAddress(5), TurnoutPosition::Closed);

    REQUIRE(std::get<TurnoutStateChanged>(event).address().value() == 5);
}

TEST_CASE("DomainCommand holds a SetTurnoutPosition")
{
    DomainCommand command = SetTurnoutPosition(TurnoutAddress(7), TurnoutPosition::Thrown);

    REQUIRE(std::get<SetTurnoutPosition>(command).address().value() == 7);
}
