#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "application/LocoNetMessageRouter.h"
#include "domain/TurnoutAddress.h"
#include "domain/TurnoutPosition.h"
#include "domain/TurnoutStateChanged.h"
#include "support/FakeClock.h"
#include "support/FakeLocoNetPort.h"
#include "support/FakeMessageLog.h"
#include "support/FakeMqttPort.h"

namespace
{
    class FakeDecoder : public LocoNetMessageDecoder
    {
    public:
        explicit FakeDecoder(uint8_t opcode) : opcode_(opcode)
        {
        }

        bool canDecode(uint8_t opcode) const override
        {
            return opcode == opcode_;
        }

        std::optional<DomainEvent> decode(const LocoNetMessage&) override
        {
            decodeCallCount_++;
            return nextResult_;
        }

        std::vector<DomainEvent> allKnownStates() const override
        {
            return knownStates_;
        }

        void setNextResult(std::optional<DomainEvent> result)
        {
            nextResult_ = result;
        }

        void setKnownStates(std::vector<DomainEvent> states)
        {
            knownStates_ = states;
        }

        int decodeCallCount() const
        {
            return decodeCallCount_;
        }

    private:
        uint8_t opcode_;
        std::optional<DomainEvent> nextResult_;
        std::vector<DomainEvent> knownStates_;
        int decodeCallCount_ = 0;
    };

    class FakeEncoder : public MqttEventEncoder
    {
    public:
        MqttMessage encode(const DomainEvent&) const override
        {
            return MqttMessage("fake/topic", "fake-payload", false);
        }
    };
}

TEST_CASE("dispatches a decoded event to the matching encoder and publishes it")
{
    FakeLocoNetPort locoNetPort;
    FakeMqttPort mqttPort;
    FakeClock clock;
    FakeMessageLog messageLog;
    FakeDecoder decoder(0xB0);
    FakeEncoder encoder;
    decoder.setNextResult(DomainEvent(TurnoutStateChanged(TurnoutAddress(5), TurnoutPosition::Closed)));
    LocoNetMessageRouter router(locoNetPort, mqttPort, clock, messageLog, {{&decoder, &encoder}});
    locoNetPort.enqueue(LocoNetMessage({0xB0, 0x04, 0x30, 0x7B}));

    router.update();

    REQUIRE(mqttPort.published().size() == 1);
    REQUIRE(mqttPort.published()[0].topic() == "fake/topic");
}

TEST_CASE("a message no decoder can decode publishes nothing")
{
    FakeLocoNetPort locoNetPort;
    FakeMqttPort mqttPort;
    FakeClock clock;
    FakeMessageLog messageLog;
    FakeDecoder decoder(0xB0);
    FakeEncoder encoder;
    LocoNetMessageRouter router(locoNetPort, mqttPort, clock, messageLog, {{&decoder, &encoder}});
    locoNetPort.enqueue(LocoNetMessage({0x81, 0x00, 0x00, 0x00}));

    router.update();

    REQUIRE(decoder.decodeCallCount() == 0);
    REQUIRE(mqttPort.published().size() == 0);
}

TEST_CASE("a decoder returning nullopt publishes nothing")
{
    FakeLocoNetPort locoNetPort;
    FakeMqttPort mqttPort;
    FakeClock clock;
    FakeMessageLog messageLog;
    FakeDecoder decoder(0xB0);
    FakeEncoder encoder;
    decoder.setNextResult(std::nullopt);
    LocoNetMessageRouter router(locoNetPort, mqttPort, clock, messageLog, {{&decoder, &encoder}});
    locoNetPort.enqueue(LocoNetMessage({0xB0, 0x04, 0x20, 0x6B}));

    router.update();

    REQUIRE(mqttPort.published().size() == 0);
}

TEST_CASE("republishes all known states once the republish interval elapses")
{
    FakeLocoNetPort locoNetPort;
    FakeMqttPort mqttPort;
    FakeClock clock;
    FakeMessageLog messageLog;
    FakeDecoder decoder(0xB0);
    FakeEncoder encoder;
    decoder.setKnownStates({DomainEvent(TurnoutStateChanged(TurnoutAddress(5), TurnoutPosition::Closed))});
    LocoNetMessageRouter router(locoNetPort, mqttPort, clock, messageLog, {{&decoder, &encoder}});
    clock.setNowMilliseconds(30000);

    router.update();

    REQUIRE(mqttPort.published().size() == 1);
}

TEST_CASE("does not republish before the republish interval elapses")
{
    FakeLocoNetPort locoNetPort;
    FakeMqttPort mqttPort;
    FakeClock clock;
    FakeMessageLog messageLog;
    FakeDecoder decoder(0xB0);
    FakeEncoder encoder;
    decoder.setKnownStates({DomainEvent(TurnoutStateChanged(TurnoutAddress(5), TurnoutPosition::Closed))});
    LocoNetMessageRouter router(locoNetPort, mqttPort, clock, messageLog, {{&decoder, &encoder}});
    clock.setNowMilliseconds(29999);

    router.update();

    REQUIRE(mqttPort.published().size() == 0);
}

TEST_CASE("dispatches to the second decoder when first decoder cannot decode the message")
{
    FakeLocoNetPort locoNetPort;
    FakeMqttPort mqttPort;
    FakeClock clock;
    FakeMessageLog messageLog;
    FakeDecoder decoder1(0xB0);
    FakeDecoder decoder2(0xB4);
    FakeEncoder encoder1;
    FakeEncoder encoder2;
    decoder2.setNextResult(DomainEvent(TurnoutStateChanged(TurnoutAddress(5), TurnoutPosition::Closed)));
    LocoNetMessageRouter router(locoNetPort, mqttPort, clock, messageLog, {{&decoder1, &encoder1}, {&decoder2, &encoder2}});
    locoNetPort.enqueue(LocoNetMessage({0xB4, 0x04, 0x30, 0x7B}));

    router.update();

    REQUIRE(decoder1.decodeCallCount() == 0);
    REQUIRE(mqttPort.published().size() == 1);
}

TEST_CASE("does not crash on empty LocoNet message")
{
    FakeLocoNetPort locoNetPort;
    FakeMqttPort mqttPort;
    FakeClock clock;
    FakeMessageLog messageLog;
    FakeDecoder decoder(0xB0);
    FakeEncoder encoder;
    LocoNetMessageRouter router(locoNetPort, mqttPort, clock, messageLog, {{&decoder, &encoder}});
    locoNetPort.enqueue(LocoNetMessage({}));

    router.update();

    REQUIRE(decoder.decodeCallCount() == 0);
}

TEST_CASE("does not crash on a too-short LocoNet message")
{
    FakeLocoNetPort locoNetPort;
    FakeMqttPort mqttPort;
    FakeClock clock;
    FakeMessageLog messageLog;
    FakeDecoder decoder(0xB0);
    FakeEncoder encoder;
    LocoNetMessageRouter router(locoNetPort, mqttPort, clock, messageLog, {{&decoder, &encoder}});
    locoNetPort.enqueue(LocoNetMessage({0xB0, 0x04, 0x30}));

    router.update();

    REQUIRE(decoder.decodeCallCount() == 0);
}

TEST_CASE("logs every drained message, including ones no decoder handles")
{
    FakeLocoNetPort locoNetPort;
    FakeMqttPort mqttPort;
    FakeClock clock;
    FakeMessageLog messageLog;
    FakeDecoder decoder(0xB0);
    FakeEncoder encoder;
    LocoNetMessageRouter router(locoNetPort, mqttPort, clock, messageLog, {{&decoder, &encoder}});
    locoNetPort.enqueue(LocoNetMessage({0xB0, 0x04, 0x30, 0x7B}));
    locoNetPort.enqueue(LocoNetMessage({0x81, 0x00, 0x00, 0x00}));

    router.update();

    REQUIRE(messageLog.recorded().size() == 2);
}
