#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "support/FakeSetupModeRequestStore.h"

TEST_CASE("consumeIfRequested is false when nothing was requested")
{
    FakeSetupModeRequestStore store;

    REQUIRE(store.consumeIfRequested() == false);
}

TEST_CASE("request then consumeIfRequested reports true exactly once")
{
    FakeSetupModeRequestStore store;
    store.request();

    REQUIRE(store.consumeIfRequested() == true);
    REQUIRE(store.consumeIfRequested() == false);
}
