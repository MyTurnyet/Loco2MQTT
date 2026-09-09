#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "support/FakeActivityIndicator.h"

TEST_CASE("a fresh FakeActivityIndicator has not flashed")
{
    FakeActivityIndicator indicator;

    REQUIRE(indicator.flashCallCount() == 0);
}

TEST_CASE("flash() increments the call count")
{
    FakeActivityIndicator indicator;

    indicator.flash();
    indicator.flash();

    REQUIRE(indicator.flashCallCount() == 2);
}
