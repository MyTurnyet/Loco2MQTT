#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "application/MqttCommandRouter.h"
#include "domain/SetTurnoutPosition.h"
#include "domain/TurnoutAddress.h"
#include "domain/TurnoutPosition.h"
#include "support/FakeLocoNetSendScheduler.h"
#include "support/FakeMqttPort.h"

namespace
{
    class FakeDecoder : public MqttCommandDecoder
    {
    public:
        explicit FakeDecoder(std::string deviceType) : deviceType_(std::move(deviceType))
        {
        }

        bool canDecode(const std::string& deviceTypeSegment) const override
        {
            return deviceTypeSegment == deviceType_;
        }

        std::optional<DomainCommand> decode(const std::string& address, const std::string& payload) const override
        {
            lastAddress_ = address;
            lastPayload_ = payload;
            return nextResult_;
        }

        void setNextResult(std::optional<DomainCommand> result)
        {
            nextResult_ = result;
        }

        const std::string& lastAddress() const
        {
            return lastAddress_;
        }

        const std::string& lastPayload() const
        {
            return lastPayload_;
        }

    private:
        std::string deviceType_;
        mutable std::optional<DomainCommand> nextResult_;
        mutable std::string lastAddress_;
        mutable std::string lastPayload_;
    };

    class FakeEncoder : public LocoNetEncoder
    {
    public:
        void encode(const DomainCommand&, LocoNetSendScheduler& scheduler) const override
        {
            encodeCallCount_++;
            scheduler.sendNow(LocoNetMessage({0xB0, 0x00, 0x00, 0x4F}));
        }

        int encodeCallCount() const
        {
            return encodeCallCount_;
        }

    private:
        mutable int encodeCallCount_ = 0;
    };
}

TEST_CASE("dispatches a decoded command to the matching encoder")
{
    FakeMqttPort mqttPort;
    FakeLocoNetSendScheduler scheduler;
    FakeDecoder decoder("turnout");
    FakeEncoder encoder;
    decoder.setNextResult(DomainCommand(SetTurnoutPosition(TurnoutAddress(5), TurnoutPosition::Closed)));
    MqttCommandRouter router(mqttPort, scheduler, {{&decoder, &encoder}});
    mqttPort.enqueueCommand(IncomingMqttMessage("loconet/turnout/5/set", "CLOSED"));

    router.update();

    REQUIRE(encoder.encodeCallCount() == 1);
    REQUIRE(decoder.lastAddress() == "5");
    REQUIRE(decoder.lastPayload() == "CLOSED");
    REQUIRE(scheduler.sentNow().size() == 1);
}

TEST_CASE("a topic no decoder can decode dispatches nothing")
{
    FakeMqttPort mqttPort;
    FakeLocoNetSendScheduler scheduler;
    FakeDecoder decoder("turnout");
    FakeEncoder encoder;
    MqttCommandRouter router(mqttPort, scheduler, {{&decoder, &encoder}});
    mqttPort.enqueueCommand(IncomingMqttMessage("loconet/sensor/5/set", "CLOSED"));

    router.update();

    REQUIRE(encoder.encodeCallCount() == 0);
}

TEST_CASE("a decoder returning nullopt dispatches nothing")
{
    FakeMqttPort mqttPort;
    FakeLocoNetSendScheduler scheduler;
    FakeDecoder decoder("turnout");
    FakeEncoder encoder;
    decoder.setNextResult(std::nullopt);
    MqttCommandRouter router(mqttPort, scheduler, {{&decoder, &encoder}});
    mqttPort.enqueueCommand(IncomingMqttMessage("loconet/turnout/5/set", "SIDEWAYS"));

    router.update();

    REQUIRE(encoder.encodeCallCount() == 0);
}

TEST_CASE("a malformed topic with too few segments dispatches nothing")
{
    FakeMqttPort mqttPort;
    FakeLocoNetSendScheduler scheduler;
    FakeDecoder decoder("turnout");
    FakeEncoder encoder;
    MqttCommandRouter router(mqttPort, scheduler, {{&decoder, &encoder}});
    mqttPort.enqueueCommand(IncomingMqttMessage("loconet/turnout", "CLOSED"));

    router.update();

    REQUIRE(encoder.encodeCallCount() == 0);
}
