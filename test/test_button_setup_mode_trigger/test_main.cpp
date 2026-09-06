#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "application/ButtonSetupModeTrigger.h"
#include "support/FakeClock.h"
#include "support/FakeDigitalInput.h"
#include "support/FakeSetupModeRequestStore.h"

TEST_CASE("never triggers while the button is inactive")
{
    FakeDigitalInput button;
    FakeClock clock;
    FakeSetupModeRequestStore requestStore;
    ButtonSetupModeTrigger trigger(button, clock, requestStore);

    REQUIRE(trigger.update() == false);
    clock.setNowMilliseconds(5000);
    REQUIRE(trigger.update() == false);
    REQUIRE(requestStore.consumeIfRequested() == false);
}

TEST_CASE("does not trigger before the hold reaches 3000ms")
{
    FakeDigitalInput button;
    FakeClock clock;
    FakeSetupModeRequestStore requestStore;
    ButtonSetupModeTrigger trigger(button, clock, requestStore);
    button.setActive(true);

    clock.setNowMilliseconds(0);
    REQUIRE(trigger.update() == false);
    clock.setNowMilliseconds(2999);
    REQUIRE(trigger.update() == false);
    REQUIRE(requestStore.consumeIfRequested() == false);
}

TEST_CASE("triggers once a continuous hold reaches exactly 3000ms and requests setup mode")
{
    FakeDigitalInput button;
    FakeClock clock;
    FakeSetupModeRequestStore requestStore;
    ButtonSetupModeTrigger trigger(button, clock, requestStore);
    button.setActive(true);

    clock.setNowMilliseconds(0);
    trigger.update();
    clock.setNowMilliseconds(3000);

    REQUIRE(trigger.update() == true);
    REQUIRE(requestStore.consumeIfRequested() == true);
}

TEST_CASE("does not trigger again while still held past the threshold")
{
    FakeDigitalInput button;
    FakeClock clock;
    FakeSetupModeRequestStore requestStore;
    ButtonSetupModeTrigger trigger(button, clock, requestStore);
    button.setActive(true);
    clock.setNowMilliseconds(0);
    trigger.update();
    clock.setNowMilliseconds(3000);
    trigger.update();

    clock.setNowMilliseconds(4000);

    REQUIRE(trigger.update() == false);
}

TEST_CASE("releasing before the threshold resets the hold, requiring a fresh 3000ms hold")
{
    FakeDigitalInput button;
    FakeClock clock;
    FakeSetupModeRequestStore requestStore;
    ButtonSetupModeTrigger trigger(button, clock, requestStore);

    clock.setNowMilliseconds(0);
    button.setActive(true);
    trigger.update();
    clock.setNowMilliseconds(1000);
    button.setActive(false);
    trigger.update();

    clock.setNowMilliseconds(1000);
    button.setActive(true);
    trigger.update();
    clock.setNowMilliseconds(3999);
    REQUIRE(trigger.update() == false);
    clock.setNowMilliseconds(4000);
    REQUIRE(trigger.update() == true);
}
