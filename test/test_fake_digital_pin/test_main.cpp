#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "support/FakeDigitalPin.h"

TEST_CASE("FakeDigitalPin begins Low")
{
    FakeDigitalPin pin;

    REQUIRE(pin.level() == Level::Low);
}

TEST_CASE("write(High) reports High")
{
    FakeDigitalPin pin;

    pin.write(Level::High);

    REQUIRE(pin.level() == Level::High);
}

TEST_CASE("write(Low) reports Low")
{
    FakeDigitalPin pin;
    pin.write(Level::High);

    pin.write(Level::Low);

    REQUIRE(pin.level() == Level::Low);
}

TEST_CASE("write() records how many times it was called")
{
    FakeDigitalPin pin;

    pin.write(Level::High);
    pin.write(Level::Low);
    pin.write(Level::High);

    REQUIRE(pin.writeCallCount() == 3);
}
