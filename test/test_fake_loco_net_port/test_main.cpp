#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "support/FakeLocoNetPort.h"

TEST_CASE("receive() returns nullopt when nothing was enqueued")
{
    FakeLocoNetPort port;

    REQUIRE(port.receive() == std::nullopt);
}

TEST_CASE("receive() returns an enqueued message")
{
    FakeLocoNetPort port;
    LocoNetMessage message({0xB2, 0x00});
    port.enqueue(message);

    REQUIRE(port.receive() == message);
}

TEST_CASE("receive() returns enqueued messages in FIFO order, then nullopt")
{
    FakeLocoNetPort port;
    port.enqueue(LocoNetMessage({0x01}));
    port.enqueue(LocoNetMessage({0x02}));

    REQUIRE(port.receive() == LocoNetMessage({0x01}));
    REQUIRE(port.receive() == LocoNetMessage({0x02}));
    REQUIRE(port.receive() == std::nullopt);
}

TEST_CASE("send() records the sent message")
{
    FakeLocoNetPort port;
    LocoNetMessage message({0xAA});

    port.send(message);

    REQUIRE(port.sent().size() == 1);
    REQUIRE(port.sent()[0] == message);
}
