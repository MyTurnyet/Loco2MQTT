#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/TurnoutAddress.h"

TEST_CASE("TurnoutAddress stores and returns its value")
{
    TurnoutAddress address(5);

    REQUIRE(address.value() == 5);
}

TEST_CASE("TurnoutAddress supports the low end of the LocoNet range")
{
    TurnoutAddress address(1);

    REQUIRE(address.value() == 1);
}

TEST_CASE("TurnoutAddress supports the high end of the LocoNet range")
{
    TurnoutAddress address(2048);

    REQUIRE(address.value() == 2048);
}
