#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/LocoNetAdapterConfig.h"

TEST_CASE("A default-constructed config is not complete")
{
    LocoNetAdapterConfig config;

    REQUIRE(config.isComplete() == false);
}

TEST_CASE("A config with only an SSID is not complete")
{
    LocoNetAdapterConfig config("MyHomeWifi", "");

    REQUIRE(config.isComplete() == false);
}

TEST_CASE("A config with only a password is not complete")
{
    LocoNetAdapterConfig config("", "hunter2");

    REQUIRE(config.isComplete() == false);
}

TEST_CASE("A config with both fields is complete")
{
    LocoNetAdapterConfig config("MyHomeWifi", "hunter2");

    REQUIRE(config.isComplete() == true);
}

TEST_CASE("withWifiSsid returns a new config with the SSID changed and password unchanged")
{
    LocoNetAdapterConfig config("Old", "hunter2");

    LocoNetAdapterConfig updated = config.withWifiSsid("New");

    REQUIRE(updated.wifiSsid() == "New");
    REQUIRE(updated.wifiPassword() == "hunter2");
}

TEST_CASE("withWifiPassword returns a new config with the password changed and SSID unchanged")
{
    LocoNetAdapterConfig config("MyHomeWifi", "old-pass");

    LocoNetAdapterConfig updated = config.withWifiPassword("new-pass");

    REQUIRE(updated.wifiSsid() == "MyHomeWifi");
    REQUIRE(updated.wifiPassword() == "new-pass");
}

TEST_CASE("Configs with equal fields compare equal")
{
    LocoNetAdapterConfig a("MyHomeWifi", "hunter2");
    LocoNetAdapterConfig b("MyHomeWifi", "hunter2");

    REQUIRE(a == b);
}

TEST_CASE("Configs with different fields compare unequal")
{
    LocoNetAdapterConfig a("MyHomeWifi", "hunter2");
    LocoNetAdapterConfig b("MyHomeWifi", "different");

    REQUIRE(a != b);
}
