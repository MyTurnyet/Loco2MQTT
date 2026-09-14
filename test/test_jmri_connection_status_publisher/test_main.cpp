#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "application/JmriConnectionStatusPublisher.h"
#include "support/FakeClock.h"
#include "support/FakeLineStream.h"
#include "support/FakeMqttPort.h"

TEST_CASE("publishes connected on the very first update when the stream is up")
{
    FakeLineStream stream;
    FakeMqttPort mqttPort;
    FakeClock clock;
    stream.setConnected(true);
    JmriConnectionStatusPublisher publisher(stream, mqttPort, clock);

    publisher.update();

    REQUIRE(mqttPort.published().size() == 1);
    REQUIRE(mqttPort.published()[0].topic() == "loco2mqtt/jmri/status");
    REQUIRE(mqttPort.published()[0].payload() == "connected");
    REQUIRE(mqttPort.published()[0].retained() == true);
}

TEST_CASE("publishes disconnected on the very first update when the stream is down")
{
    FakeLineStream stream;
    FakeMqttPort mqttPort;
    FakeClock clock;
    stream.setConnected(false);
    JmriConnectionStatusPublisher publisher(stream, mqttPort, clock);

    publisher.update();

    REQUIRE(mqttPort.published().size() == 1);
    REQUIRE(mqttPort.published()[0].payload() == "disconnected");
}

TEST_CASE("does not republish when nothing changed and the heartbeat isn't due")
{
    FakeLineStream stream;
    FakeMqttPort mqttPort;
    FakeClock clock;
    JmriConnectionStatusPublisher publisher(stream, mqttPort, clock);
    publisher.update();

    publisher.update();

    REQUIRE(mqttPort.published().size() == 1);
}

TEST_CASE("publishes again immediately when the connection state changes")
{
    FakeLineStream stream;
    FakeMqttPort mqttPort;
    FakeClock clock;
    stream.setConnected(true);
    JmriConnectionStatusPublisher publisher(stream, mqttPort, clock);
    publisher.update();

    stream.setConnected(false);
    publisher.update();

    REQUIRE(mqttPort.published().size() == 2);
    REQUIRE(mqttPort.published()[1].payload() == "disconnected");
}

TEST_CASE("republishes the same state once the heartbeat interval elapses")
{
    FakeLineStream stream;
    FakeMqttPort mqttPort;
    FakeClock clock;
    stream.setConnected(true);
    JmriConnectionStatusPublisher publisher(stream, mqttPort, clock);
    publisher.update();

    clock.setNowMilliseconds(30000);
    publisher.update();

    REQUIRE(mqttPort.published().size() == 2);
    REQUIRE(mqttPort.published()[1].payload() == "connected");
}

TEST_CASE("does not republish early, just before the heartbeat interval elapses")
{
    FakeLineStream stream;
    FakeMqttPort mqttPort;
    FakeClock clock;
    JmriConnectionStatusPublisher publisher(stream, mqttPort, clock);
    publisher.update();

    clock.setNowMilliseconds(29999);
    publisher.update();

    REQUIRE(mqttPort.published().size() == 1);
}