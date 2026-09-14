#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "application/LocoNetMessageRouter.h"
#include "application/MqttCommandRouter.h"
#include "application/PendingLocoNetSendScheduler.h"
#include "application/TurnoutTableStartupQuery.h"
#include "support/FakeClock.h"
#include "support/FakeLineStream.h"
#include "support/FakeLocoNetPort.h"
#include "support/FakeMessageLog.h"
#include "support/FakeMqttPort.h"
#include "turnout/PendingTurnoutStateAcks.h"
#include "turnout/TurnoutLocoNetDecoder.h"
#include "turnout/TurnoutLocoNetEncoder.h"
#include "turnout/TurnoutMqttCommandDecoder.h"
#include "turnout/TurnoutMqttEncoder.h"

TEST_CASE("read pipeline: real LocoNet bytes reach a published MQTT message")
{
    FakeLocoNetPort locoNetPort;
    FakeMqttPort mqttPort;
    FakeClock clock;
    FakeMessageLog messageLog;
    PendingTurnoutStateAcks pendingAcks;
    TurnoutLocoNetDecoder decoder(pendingAcks);
    TurnoutMqttEncoder encoder;
    LocoNetMessageRouter router(locoNetPort, mqttPort, clock, messageLog, {{&decoder, &encoder}});
    locoNetPort.enqueue(LocoNetMessage({0xB0, 0x04, 0x30, 0x7B}));

    router.update();

    REQUIRE(mqttPort.published().size() == 1);
    REQUIRE(mqttPort.published()[0].topic() == "loconet/turnout/5/state");
    REQUIRE(mqttPort.published()[0].payload() == "CLOSED");
    REQUIRE(mqttPort.published()[0].retained() == true);
    REQUIRE(messageLog.recorded().size() == 1);
}

TEST_CASE("write pipeline: a real MQTT command reaches on-wire LocoNet bytes")
{
    FakeMqttPort mqttPort;
    FakeLocoNetPort locoNetPort;
    FakeClock clock;
    PendingLocoNetSendScheduler scheduler(locoNetPort, clock);
    TurnoutMqttCommandDecoder decoder;
    TurnoutLocoNetEncoder encoder;
    MqttCommandRouter router(mqttPort, scheduler, {{&decoder, &encoder}});
    mqttPort.enqueueCommand(IncomingMqttMessage("loconet/turnout/5/set", "CLOSED"));

    router.update();

    REQUIRE(locoNetPort.sent().size() == 1);
    REQUIRE(locoNetPort.sent()[0].bytes() == std::vector<uint8_t>{0xB0, 0x04, 0x30, 0x7B});

    clock.setNowMilliseconds(250);
    scheduler.update();

    REQUIRE(locoNetPort.sent().size() == 2);
    REQUIRE(locoNetPort.sent()[1].bytes() == std::vector<uint8_t>{0xB0, 0x04, 0x20, 0x6B});
}

// Reproduces Paige's 2026-09-14 on-hardware capture: OPC_SW_STATE for
// address 1 ([BC 00 00 43]) answered by OPC_LONG_ACK ([B4 3C 30 47],
// Closed) rather than OPC_SW_REP -- the bug that motivated
// PendingTurnoutStateAcks and TurnoutLocoNetDecoder's OPC_LONG_ACK
// handling (see ADR 0001's addendum).
TEST_CASE("startup query pipeline: a real OPC_LONG_ACK reply reaches a published MQTT message")
{
    FakeLineStream stream;
    FakeLocoNetPort locoNetPort;
    FakeMqttPort mqttPort;
    FakeClock clock;
    FakeMessageLog messageLog;
    PendingTurnoutStateAcks pendingAcks;
    PendingLocoNetSendScheduler scheduler(locoNetPort, clock);
    TurnoutTableStartupQuery startupQuery(stream, scheduler, pendingAcks, clock);
    TurnoutLocoNetDecoder decoder(pendingAcks);
    TurnoutMqttEncoder encoder;
    LocoNetMessageRouter router(locoNetPort, mqttPort, clock, messageLog, {{&decoder, &encoder}});

    stream.setConnected(true);
    startupQuery.update();
    clock.setNowMilliseconds(2000);
    startupQuery.update();
    clock.setNowMilliseconds(2000 + 47 * 20);
    scheduler.update();

    REQUIRE(locoNetPort.sent().front().bytes() == std::vector<uint8_t>{0xBC, 0x00, 0x00, 0x43});

    locoNetPort.enqueue(LocoNetMessage({0xB4, 0x3C, 0x30, 0x47}));
    router.update();

    REQUIRE(mqttPort.published().size() == 1);
    REQUIRE(mqttPort.published()[0].topic() == "loconet/turnout/1/state");
    REQUIRE(mqttPort.published()[0].payload() == "CLOSED");
}
