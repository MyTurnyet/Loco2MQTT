#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "application/LocoNetMessageLogger.h"
#include "support/FakeLocoNetPort.h"
#include "support/FakeMessageLog.h"

TEST_CASE("update() does nothing when the port has no message")
{
    FakeLocoNetPort port;
    FakeMessageLog log;
    LocoNetMessageLogger logger(port, log);

    logger.update();

    REQUIRE(log.recorded().empty());
}

TEST_CASE("update() forwards a received message to the log")
{
    FakeLocoNetPort port;
    FakeMessageLog log;
    LocoNetMessageLogger logger(port, log);
    LocoNetMessage message({0xB2, 0x00, 0x00, 0x50});
    port.enqueue(message);

    logger.update();

    REQUIRE(log.recorded().size() == 1);
    REQUIRE(log.recorded()[0] == message);
}

TEST_CASE("update() forwards exactly one message per call")
{
    FakeLocoNetPort port;
    FakeMessageLog log;
    LocoNetMessageLogger logger(port, log);
    port.enqueue(LocoNetMessage({0x01}));
    port.enqueue(LocoNetMessage({0x02}));

    logger.update();
    logger.update();

    REQUIRE(log.recorded().size() == 2);
    REQUIRE(log.recorded()[0] == LocoNetMessage({0x01}));
    REQUIRE(log.recorded()[1] == LocoNetMessage({0x02}));
}
