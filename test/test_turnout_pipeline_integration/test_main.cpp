#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "application/LocoNetMessageRouter.h"
#include "application/MqttCommandRouter.h"
#include "application/PendingLocoNetSendScheduler.h"
#include "support/FakeClock.h"
#include "support/FakeLocoNetPort.h"
#include "support/FakeMessageLog.h"
#include "support/FakeMqttPort.h"
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
    TurnoutLocoNetDecoder decoder;
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
}
