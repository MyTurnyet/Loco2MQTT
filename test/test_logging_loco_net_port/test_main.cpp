#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "application/LoggingLocoNetPort.h"
#include "support/FakeLocoNetPort.h"
#include "support/FakeMessageLog.h"

TEST_CASE("send forwards the message to the wrapped port")
{
    FakeLocoNetPort inner;
    FakeMessageLog sentLog;
    LoggingLocoNetPort port(inner, sentLog);
    const LocoNetMessage message({0xB0, 0x04, 0x20, 0x6B});

    port.send(message);

    REQUIRE(inner.sent().size() == 1);
    REQUIRE(inner.sent()[0] == message);
}

TEST_CASE("send records the message to the sent log")
{
    FakeLocoNetPort inner;
    FakeMessageLog sentLog;
    LoggingLocoNetPort port(inner, sentLog);
    const LocoNetMessage message({0xB0, 0x04, 0x20, 0x6B});

    port.send(message);

    REQUIRE(sentLog.recorded().size() == 1);
    REQUIRE(sentLog.recorded()[0] == message);
}

TEST_CASE("receive passes messages through without recording them")
{
    FakeLocoNetPort inner;
    FakeMessageLog sentLog;
    LoggingLocoNetPort port(inner, sentLog);
    const LocoNetMessage message({0xBC, 0x00, 0x00, 0x43});
    inner.enqueue(message);

    const auto received = port.receive();

    REQUIRE(received.has_value());
    REQUIRE(*received == message);
    REQUIRE(sentLog.recorded().empty());
}
