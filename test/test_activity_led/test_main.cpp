#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "application/ActivityLed.h"
#include "support/FakeClock.h"
#include "support/FakeDigitalPin.h"

TEST_CASE("a fresh ActivityLed leaves the pin Low")
{
    FakeDigitalPin pin;
    FakeClock clock;
    ActivityLed led(pin, clock, 40);

    REQUIRE(pin.level() == Level::Low);
}

TEST_CASE("flash() turns the pin High")
{
    FakeDigitalPin pin;
    FakeClock clock;
    ActivityLed led(pin, clock, 40);

    led.flash();

    REQUIRE(pin.level() == Level::High);
}

TEST_CASE("update() leaves the pin High before the flash duration elapses")
{
    FakeDigitalPin pin;
    FakeClock clock;
    ActivityLed led(pin, clock, 40);
    clock.setNowMilliseconds(1000);
    led.flash();

    clock.setNowMilliseconds(1039);
    led.update();

    REQUIRE(pin.level() == Level::High);
}

TEST_CASE("update() turns the pin Low once the flash duration elapses")
{
    FakeDigitalPin pin;
    FakeClock clock;
    ActivityLed led(pin, clock, 40);
    clock.setNowMilliseconds(1000);
    led.flash();

    clock.setNowMilliseconds(1040);
    led.update();

    REQUIRE(pin.level() == Level::Low);
}

TEST_CASE("update() without a prior flash() never writes the pin")
{
    FakeDigitalPin pin;
    FakeClock clock;
    ActivityLed led(pin, clock, 40);

    led.update();

    REQUIRE(pin.writeCallCount() == 0);
}

TEST_CASE("a second flash() before the first expires restarts the flash window")
{
    FakeDigitalPin pin;
    FakeClock clock;
    ActivityLed led(pin, clock, 40);
    clock.setNowMilliseconds(1000);
    led.flash();

    clock.setNowMilliseconds(1030);
    led.flash();

    clock.setNowMilliseconds(1040);
    led.update();
    REQUIRE(pin.level() == Level::High);

    clock.setNowMilliseconds(1070);
    led.update();
    REQUIRE(pin.level() == Level::Low);
}
