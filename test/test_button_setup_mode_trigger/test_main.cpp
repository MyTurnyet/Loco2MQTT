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

TEST_CASE("does not trigger merely by holding — release is required")
{
    FakeDigitalInput button;
    FakeClock clock;
    FakeSetupModeRequestStore requestStore;
    ButtonSetupModeTrigger trigger(button, clock, requestStore);
    button.setActive(true);

    clock.setNowMilliseconds(0);
    trigger.update();
    clock.setNowMilliseconds(4000);
    REQUIRE(trigger.update() == false);
    REQUIRE(requestStore.consumeIfRequested() == false);
}

TEST_CASE("releasing after a 3-second hold triggers once the 50ms settle window elapses")
{
    FakeDigitalInput button;
    FakeClock clock;
    FakeSetupModeRequestStore requestStore;
    ButtonSetupModeTrigger trigger(button, clock, requestStore);

    clock.setNowMilliseconds(0);
    button.setActive(true);
    trigger.update();
    clock.setNowMilliseconds(3000);
    button.setActive(false);
    trigger.update();
    clock.setNowMilliseconds(3049);
    REQUIRE(trigger.update() == false);
    clock.setNowMilliseconds(3050);
    REQUIRE(trigger.update() == true);
    REQUIRE(requestStore.consumeIfRequested() == true);
}

TEST_CASE("releasing before 3 seconds does not trigger even after the settle window")
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
    clock.setNowMilliseconds(1050);
    REQUIRE(trigger.update() == false);
    REQUIRE(requestStore.consumeIfRequested() == false);
}

TEST_CASE("a brief bounce back to active during the settle window does not reset the original hold start")
{
    FakeDigitalInput button;
    FakeClock clock;
    FakeSetupModeRequestStore requestStore;
    ButtonSetupModeTrigger trigger(button, clock, requestStore);

    clock.setNowMilliseconds(0);
    button.setActive(true);
    trigger.update();
    clock.setNowMilliseconds(3000);
    button.setActive(false);
    trigger.update();
    clock.setNowMilliseconds(3020);
    button.setActive(true);
    trigger.update();
    clock.setNowMilliseconds(3021);
    button.setActive(false);
    trigger.update();
    clock.setNowMilliseconds(3071);
    REQUIRE(trigger.update() == true);
    REQUIRE(requestStore.consumeIfRequested() == true);
}

TEST_CASE("triggering once requires a fresh full press-release cycle to trigger again")
{
    FakeDigitalInput button;
    FakeClock clock;
    FakeSetupModeRequestStore requestStore;
    ButtonSetupModeTrigger trigger(button, clock, requestStore);
    clock.setNowMilliseconds(0);
    button.setActive(true);
    trigger.update();
    clock.setNowMilliseconds(3000);
    button.setActive(false);
    trigger.update();
    clock.setNowMilliseconds(3050);
    trigger.update();
    requestStore.consumeIfRequested();

    clock.setNowMilliseconds(3060);
    REQUIRE(trigger.update() == false);
}
