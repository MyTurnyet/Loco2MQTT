#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/BootMode.h"
#include "domain/LocoNetAdapterConfig.h"

TEST_CASE("a pending setup request wins even with a complete config")
{
    LocoNetAdapterConfig config("MyHomeWifi", "hunter2", "192.168.1.13", 1234);

    REQUIRE(selectBootMode(config, true) == BootMode::WirelessSetup);
}

TEST_CASE("a pending setup request wins even with an incomplete config")
{
    LocoNetAdapterConfig config;

    REQUIRE(selectBootMode(config, true) == BootMode::WirelessSetup);
}

TEST_CASE("no setup request and an incomplete config needs commissioning")
{
    LocoNetAdapterConfig config;

    REQUIRE(selectBootMode(config, false) == BootMode::NeedsCommissioning);
}

TEST_CASE("no setup request and a complete config boots normally")
{
    LocoNetAdapterConfig config("MyHomeWifi", "hunter2", "192.168.1.13", 1234);

    REQUIRE(selectBootMode(config, false) == BootMode::Normal);
}

TEST_CASE("no setup request and WiFi-only fields (no JMRI host/port) needs commissioning")
{
    LocoNetAdapterConfig config("MyHomeWifi", "hunter2");

    REQUIRE(selectBootMode(config, false) == BootMode::NeedsCommissioning);
}
