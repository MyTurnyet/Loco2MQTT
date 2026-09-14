#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "application/TurnoutTableStartupQuery.h"
#include "support/FakeLineStream.h"
#include "support/FakeLocoNetSendScheduler.h"
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
    stream.setConnected(false);
    TurnoutTableStartupQuery query(stream, scheduler);

    query.update();

    REQUIRE(scheduler.scheduled().empty());
}

TEST_CASE("queries every configured address once the stream connects")
{
    FakeLineStream stream;
    FakeLocoNetSendScheduler scheduler;
    stream.setConnected(false);
    TurnoutTableStartupQuery query(stream, scheduler);
    query.update();

    stream.setConnected(true);
    query.update();

    REQUIRE(scheduler.scheduled().size() == 48);
    REQUIRE(scheduler.scheduled().front().first == expectedRequest(1));
    REQUIRE(scheduler.scheduled().back().first == expectedRequest(48));
}

TEST_CASE("staggers each request 20ms apart so the bus isn't flooded")
{
    FakeLineStream stream;
    FakeLocoNetSendScheduler scheduler;
    stream.setConnected(true);
    TurnoutTableStartupQuery query(stream, scheduler);

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
    stream.setConnected(true);
    TurnoutTableStartupQuery query(stream, scheduler);
    query.update();

    query.update();

    REQUIRE(scheduler.scheduled().size() == 48);
}

TEST_CASE("queries again after a disconnect/reconnect cycle -- the 'reset update'")
{
    FakeLineStream stream;
    FakeLocoNetSendScheduler scheduler;
    stream.setConnected(true);
    TurnoutTableStartupQuery query(stream, scheduler);
    query.update();

    stream.setConnected(false);
    query.update();
    stream.setConnected(true);
    query.update();

    REQUIRE(scheduler.scheduled().size() == 96);
}
