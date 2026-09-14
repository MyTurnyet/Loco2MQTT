#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "application/TurnoutTableStartupQuery.h"
#include "support/FakeClock.h"
#include "support/FakeLineStream.h"
#include "support/FakeLocoNetSendScheduler.h"
#include "turnout/PendingTurnoutStateAcks.h"
#include "turnout/TurnoutStateRequestEncoder.h"

namespace
{
    LocoNetMessage expectedRequest(int address)
    {
        return TurnoutStateRequestEncoder().encode(TurnoutAddress(address));
    }
}

TEST_CASE("queries nothing while the stream has never been connected")
{
    FakeLineStream stream;
    FakeLocoNetSendScheduler scheduler;
    PendingTurnoutStateAcks pendingAcks;
    FakeClock clock;
    stream.setConnected(false);
    TurnoutTableStartupQuery query(stream, scheduler, pendingAcks, clock);

    query.update();

    REQUIRE(scheduler.scheduled().empty());
    REQUIRE(pendingAcks.claimNext().has_value() == false);
}

TEST_CASE("does not query the instant the connection comes up -- waits for it to be stable")
{
    FakeLineStream stream;
    FakeLocoNetSendScheduler scheduler;
    PendingTurnoutStateAcks pendingAcks;
    FakeClock clock;
    stream.setConnected(true);
    TurnoutTableStartupQuery query(stream, scheduler, pendingAcks, clock);

    query.update();

    REQUIRE(scheduler.scheduled().empty());
}

TEST_CASE("queries every configured address once the connection has been stable for 2 seconds")
{
    FakeLineStream stream;
    FakeLocoNetSendScheduler scheduler;
    PendingTurnoutStateAcks pendingAcks;
    FakeClock clock;
    stream.setConnected(true);
    TurnoutTableStartupQuery query(stream, scheduler, pendingAcks, clock);
    query.update();

    clock.setNowMilliseconds(2000);
    query.update();

    REQUIRE(scheduler.scheduled().size() == 48);
    REQUIRE(scheduler.scheduled().front().first == expectedRequest(1));
    REQUIRE(scheduler.scheduled().back().first == expectedRequest(48));
}

TEST_CASE("does not query yet just short of the stability window")
{
    FakeLineStream stream;
    FakeLocoNetSendScheduler scheduler;
    PendingTurnoutStateAcks pendingAcks;
    FakeClock clock;
    stream.setConnected(true);
    TurnoutTableStartupQuery query(stream, scheduler, pendingAcks, clock);
    query.update();

    clock.setNowMilliseconds(1999);
    query.update();

    REQUIRE(scheduler.scheduled().empty());
}

TEST_CASE("a brief blip back to disconnected resets the stability timer instead of firing early")
{
    FakeLineStream stream;
    FakeLocoNetSendScheduler scheduler;
    PendingTurnoutStateAcks pendingAcks;
    FakeClock clock;
    stream.setConnected(true);
    TurnoutTableStartupQuery query(stream, scheduler, pendingAcks, clock);
    query.update();

    clock.setNowMilliseconds(1000);
    stream.setConnected(false);
    query.update();
    stream.setConnected(true);
    query.update();
    clock.setNowMilliseconds(2000);
    query.update();

    REQUIRE(scheduler.scheduled().empty());

    clock.setNowMilliseconds(3000);
    query.update();

    REQUIRE(scheduler.scheduled().size() == 48);
}

TEST_CASE("repeated flapping that never stays connected for the full window never queries at all")
{
    FakeLineStream stream;
    FakeLocoNetSendScheduler scheduler;
    PendingTurnoutStateAcks pendingAcks;
    FakeClock clock;
    TurnoutTableStartupQuery query(stream, scheduler, pendingAcks, clock);

    for (unsigned long t = 0; t < 10000; t += 500)
    {
        clock.setNowMilliseconds(t);
        stream.setConnected(t % 1000 == 0);
        query.update();
    }

    REQUIRE(scheduler.scheduled().empty());
}

TEST_CASE("also queues an ack expectation for every address queried, in the same order")
{
    FakeLineStream stream;
    FakeLocoNetSendScheduler scheduler;
    PendingTurnoutStateAcks pendingAcks;
    FakeClock clock;
    stream.setConnected(true);
    TurnoutTableStartupQuery query(stream, scheduler, pendingAcks, clock);
    query.update();

    clock.setNowMilliseconds(2000);
    query.update();

    REQUIRE(pendingAcks.claimNext()->value() == 1);
    REQUIRE(pendingAcks.claimNext()->value() == 2);
}

TEST_CASE("staggers each request 20ms apart so the bus isn't flooded")
{
    FakeLineStream stream;
    FakeLocoNetSendScheduler scheduler;
    PendingTurnoutStateAcks pendingAcks;
    FakeClock clock;
    stream.setConnected(true);
    TurnoutTableStartupQuery query(stream, scheduler, pendingAcks, clock);
    query.update();
    clock.setNowMilliseconds(2000);

    query.update();

    REQUIRE(scheduler.scheduled()[0].second == 0);
    REQUIRE(scheduler.scheduled()[1].second == 20);
    REQUIRE(scheduler.scheduled()[2].second == 40);
    REQUIRE(scheduler.scheduled().back().second == 47 * 20);
}

TEST_CASE("does not requery again on a later update while still connected")
{
    FakeLineStream stream;
    FakeLocoNetSendScheduler scheduler;
    PendingTurnoutStateAcks pendingAcks;
    FakeClock clock;
    stream.setConnected(true);
    TurnoutTableStartupQuery query(stream, scheduler, pendingAcks, clock);
    query.update();
    clock.setNowMilliseconds(2000);
    query.update();

    clock.setNowMilliseconds(60000);
    query.update();

    REQUIRE(scheduler.scheduled().size() == 48);
}

TEST_CASE("queries again after a disconnect/reconnect cycle -- the 'reset update'")
{
    FakeLineStream stream;
    FakeLocoNetSendScheduler scheduler;
    PendingTurnoutStateAcks pendingAcks;
    FakeClock clock;
    stream.setConnected(true);
    TurnoutTableStartupQuery query(stream, scheduler, pendingAcks, clock);
    query.update();
    clock.setNowMilliseconds(2000);
    query.update();

    stream.setConnected(false);
    query.update();
    stream.setConnected(true);
    clock.setNowMilliseconds(2100);
    query.update();
    clock.setNowMilliseconds(4100);
    query.update();

    REQUIRE(scheduler.scheduled().size() == 96);
}
