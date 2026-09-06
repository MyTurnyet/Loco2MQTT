#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "support/FakeUartPort.h"

TEST_CASE("readLine returns nullopt when nothing is queued")
{
    FakeUartPort uart;

    REQUIRE(uart.readLine() == std::nullopt);
}

TEST_CASE("readLine returns an enqueued line, then nullopt again")
{
    FakeUartPort uart;
    uart.enqueueLine("hello");

    REQUIRE(uart.readLine() == std::optional<std::string>("hello"));
    REQUIRE(uart.readLine() == std::nullopt);
}

TEST_CASE("enqueued lines are read back in FIFO order")
{
    FakeUartPort uart;
    uart.enqueueLine("first");
    uart.enqueueLine("second");

    REQUIRE(uart.readLine() == std::optional<std::string>("first"));
    REQUIRE(uart.readLine() == std::optional<std::string>("second"));
}

TEST_CASE("writeLine records written lines in order")
{
    FakeUartPort uart;

    uart.writeLine("OK");
    uart.writeLine("SAVED");

    REQUIRE(uart.writtenLines() == std::vector<std::string>{"OK", "SAVED"});
}
