#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "application/TurnoutTableStartupQuery.h"
#include "support/FakeClock.h"
#include "support/FakeLocoNetSendScheduler.h"
#include "turnout/PendingTurnoutStateAcks.h"
#include "turnout/TurnoutStateRequestEncoder.h"

namespace
{
    LocoNetMessage expectedRequest(int address)
    {
        return TurnoutStateRequestEncoder().encode(TurnoutAddress(address));
    }

    struct Fixture
    {
        FakeLocoNetSendScheduler scheduler;
        PendingTurnoutStateAcks pendingAcks;
        FakeClock clock;
        TurnoutTableStartupQuery query{scheduler, pendingAcks, clock};
    };
}

TEST_CASE("does not query the instant it starts -- waits for the startup delay")
{
    Fixture f;

    f.query.update();

    REQUIRE(f.scheduler.scheduled().empty());
}

TEST_CASE("does not query yet just short of the startup delay")
{
    Fixture f;
    f.query.update();

    f.clock.setNowMilliseconds(1999);
    f.query.update();

    REQUIRE(f.scheduler.scheduled().empty());
}

TEST_CASE("queries every configured address once the startup delay has elapsed")
{
    Fixture f;
    f.query.update();

    f.clock.setNowMilliseconds(2000);
    f.query.update();

    REQUIRE(f.scheduler.scheduled().size() == 48);
    REQUIRE(f.scheduler.scheduled().front().first == expectedRequest(1));
    REQUIRE(f.scheduler.scheduled().back().first == expectedRequest(48));
}

TEST_CASE("also queues an ack expectation for every address queried, in the same order")
{
    Fixture f;
    f.query.update();

    f.clock.setNowMilliseconds(2000);
    f.query.update();

    REQUIRE(f.pendingAcks.claimNext()->value() == 1);
    REQUIRE(f.pendingAcks.claimNext()->value() == 2);
}

TEST_CASE("staggers each request 20ms apart so the bus isn't flooded")
{
    Fixture f;
    f.query.update();
    f.clock.setNowMilliseconds(2000);

    f.query.update();

    REQUIRE(f.scheduler.scheduled()[0].second == 0);
    REQUIRE(f.scheduler.scheduled()[1].second == 20);
    REQUIRE(f.scheduler.scheduled()[2].second == 40);
    REQUIRE(f.scheduler.scheduled().back().second == 47 * 20);
}

TEST_CASE("queries only once per boot")
{
    Fixture f;
    f.query.update();
    f.clock.setNowMilliseconds(2000);
    f.query.update();

    f.clock.setNowMilliseconds(60000);
    f.query.update();

    REQUIRE(f.scheduler.scheduled().size() == 48);
}
