#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "support/FakeMessageLog.h"

TEST_CASE("A fresh FakeMessageLog has recorded nothing")
{
    FakeMessageLog log;

    REQUIRE(log.recorded().empty());
}

TEST_CASE("record() captures the message")
{
    FakeMessageLog log;
    LocoNetMessage message({0xB2, 0x00});

    log.record(message);

    REQUIRE(log.recorded().size() == 1);
    REQUIRE(log.recorded()[0] == message);
}
