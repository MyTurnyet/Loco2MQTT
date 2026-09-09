#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "application/FlashingMessageLog.h"
#include "support/FakeActivityIndicator.h"
#include "support/FakeMessageLog.h"

TEST_CASE("record() forwards the message to the wrapped log")
{
    FakeMessageLog inner;
    FakeActivityIndicator indicator;
    FlashingMessageLog log(inner, indicator);
    LocoNetMessage message({0xB2, 0x00, 0x00, 0x50});

    log.record(message);

    REQUIRE(inner.recorded().size() == 1);
    REQUIRE(inner.recorded()[0] == message);
}

TEST_CASE("record() triggers the activity indicator")
{
    FakeMessageLog inner;
    FakeActivityIndicator indicator;
    FlashingMessageLog log(inner, indicator);

    log.record(LocoNetMessage({0xB2, 0x00, 0x00, 0x50}));

    REQUIRE(indicator.flashCallCount() == 1);
}

TEST_CASE("each recorded message triggers its own flash")
{
    FakeMessageLog inner;
    FakeActivityIndicator indicator;
    FlashingMessageLog log(inner, indicator);

    log.record(LocoNetMessage({0x01}));
    log.record(LocoNetMessage({0x02}));

    REQUIRE(indicator.flashCallCount() == 2);
    REQUIRE(inner.recorded().size() == 2);
}
