#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "application/PendingLocoNetSendScheduler.h"
#include "support/FakeClock.h"
#include "support/FakeLocoNetPort.h"

TEST_CASE("sendNow sends immediately")
{
    FakeLocoNetPort port;
    FakeClock clock;
    PendingLocoNetSendScheduler scheduler(port, clock);

    scheduler.sendNow(LocoNetMessage({0xB0, 0x04, 0x30, 0x7B}));

    REQUIRE(port.sent().size() == 1);
}

TEST_CASE("sendAfter does not send before the delay has elapsed")
{
    FakeLocoNetPort port;
    FakeClock clock;
    PendingLocoNetSendScheduler scheduler(port, clock);
    clock.setNowMilliseconds(1000);

    scheduler.sendAfter(LocoNetMessage({0xB0, 0x04, 0x20, 0x6B}), 250);
    clock.setNowMilliseconds(1249);
    scheduler.update();

    REQUIRE(port.sent().size() == 0);
}

TEST_CASE("sendAfter sends once the delay has elapsed")
{
    FakeLocoNetPort port;
    FakeClock clock;
    PendingLocoNetSendScheduler scheduler(port, clock);
    clock.setNowMilliseconds(1000);

    scheduler.sendAfter(LocoNetMessage({0xB0, 0x04, 0x20, 0x6B}), 250);
    clock.setNowMilliseconds(1250);
    scheduler.update();

    REQUIRE(port.sent().size() == 1);
    REQUIRE(port.sent()[0].bytes() == std::vector<uint8_t>{0xB0, 0x04, 0x20, 0x6B});
}

TEST_CASE("a due send is only sent once across repeated update calls")
{
    FakeLocoNetPort port;
    FakeClock clock;
    PendingLocoNetSendScheduler scheduler(port, clock);

    scheduler.sendAfter(LocoNetMessage({0xB0, 0x04, 0x20, 0x6B}), 250);
    clock.setNowMilliseconds(250);
    scheduler.update();
    scheduler.update();

    REQUIRE(port.sent().size() == 1);
}

TEST_CASE("multiple pending sends each fire at their own due time")
{
    FakeLocoNetPort port;
    FakeClock clock;
    PendingLocoNetSendScheduler scheduler(port, clock);

    scheduler.sendAfter(LocoNetMessage({0xB0, 0x04, 0x20, 0x6B}), 100);
    scheduler.sendAfter(LocoNetMessage({0xB0, 0x08, 0x20, 0x67}), 300);
    clock.setNowMilliseconds(100);
    scheduler.update();

    REQUIRE(port.sent().size() == 1);

    clock.setNowMilliseconds(300);
    scheduler.update();

    REQUIRE(port.sent().size() == 2);
}
