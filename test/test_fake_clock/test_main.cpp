#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "support/FakeClock.h"

TEST_CASE("A fake clock begins at zero milliseconds")
{
    FakeClock clock;

    REQUIRE(clock.nowMilliseconds() == 0UL);
}

TEST_CASE("setNowMilliseconds changes what nowMilliseconds reports")
{
    FakeClock clock;

    clock.setNowMilliseconds(4200);

    REQUIRE(clock.nowMilliseconds() == 4200UL);
}
