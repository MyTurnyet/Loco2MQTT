#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "support/FakeDigitalInput.h"

TEST_CASE("A fake digital input begins inactive")
{
    FakeDigitalInput input;

    REQUIRE(input.isActive() == false);
}

TEST_CASE("setActive(true) makes isActive() report true")
{
    FakeDigitalInput input;

    input.setActive(true);

    REQUIRE(input.isActive() == true);
}

TEST_CASE("setActive(false) makes isActive() report false again")
{
    FakeDigitalInput input;
    input.setActive(true);

    input.setActive(false);

    REQUIRE(input.isActive() == false);
}
