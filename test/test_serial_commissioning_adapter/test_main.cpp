#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include <string>

#include "adapters/SerialCommissioningAdapter.h"
#include "application/CommissioningSession.h"
#include "domain/LocoNetAdapterConfig.h"
#include "support/FakeConfigStore.h"
#include "support/FakeUartPort.h"

TEST_CASE("update does nothing when no line is available")
{
    FakeUartPort uart;
    FakeConfigStore store;
    CommissioningSession session(store);
    SerialCommissioningAdapter adapter(uart, session);

    adapter.update();

    REQUIRE(uart.writtenLines().empty());
}

TEST_CASE("update parses a queued line and writes the session's reply")
{
    FakeUartPort uart;
    FakeConfigStore store;
    CommissioningSession session(store);
    SerialCommissioningAdapter adapter(uart, session);
    uart.enqueueLine("set-ssid MyHomeWifi");

    adapter.update();

    REQUIRE(uart.writtenLines() == std::vector<std::string>{"OK"});
}

TEST_CASE("update supports a full set-and-save round trip")
{
    FakeUartPort uart;
    FakeConfigStore store;
    CommissioningSession session(store);
    SerialCommissioningAdapter adapter(uart, session);
    uart.enqueueLine("set-ssid MyHomeWifi");
    uart.enqueueLine("set-password hunter2");
    uart.enqueueLine("save");

    adapter.update();
    adapter.update();
    adapter.update();

    REQUIRE(uart.writtenLines() == std::vector<std::string>{"OK", "OK", "SAVED"});
    REQUIRE(store.load() == LocoNetAdapterConfig("MyHomeWifi", "hunter2"));
}

TEST_CASE("update rejects a line longer than kMaxLineLength as unknown, without echoing it back")
{
    FakeUartPort uart;
    FakeConfigStore store;
    CommissioningSession session(store);
    SerialCommissioningAdapter adapter(uart, session);
    const std::string overlong(129, 'a');
    uart.enqueueLine(overlong);

    adapter.update();

    REQUIRE(uart.writtenLines() == std::vector<std::string>{"ERR unknown command"});
}

TEST_CASE("update reports an unrecognized command")
{
    FakeUartPort uart;
    FakeConfigStore store;
    CommissioningSession session(store);
    SerialCommissioningAdapter adapter(uart, session);
    uart.enqueueLine("bogus");

    adapter.update();

    REQUIRE(uart.writtenLines() == std::vector<std::string>{"ERR unknown command"});
}
