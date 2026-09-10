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

TEST_CASE("A config with WiFi fields set but no JMRI host is not complete")
{
    LocoNetAdapterConfig config("MyHomeWifi", "hunter2", "", 1234);

    REQUIRE(config.isComplete() == false);
}

TEST_CASE("A config with WiFi fields and a JMRI host but no JMRI port is not complete")
{
    LocoNetAdapterConfig config("MyHomeWifi", "hunter2", "192.168.1.13", 0);

    REQUIRE(config.isComplete() == false);
}

TEST_CASE("A config with all four fields is complete")
{
    LocoNetAdapterConfig config("MyHomeWifi", "hunter2", "192.168.1.13", 1234);

    REQUIRE(config.isComplete() == true);
}

TEST_CASE("withWifiSsid returns a new config with the SSID changed and everything else unchanged")
{
    LocoNetAdapterConfig config("Old", "hunter2", "192.168.1.13", 1234);

    LocoNetAdapterConfig updated = config.withWifiSsid("New");

    REQUIRE(updated.wifiSsid() == "New");
    REQUIRE(updated.wifiPassword() == "hunter2");
    REQUIRE(updated.jmriHost() == "192.168.1.13");
    REQUIRE(updated.jmriPort() == 1234);
}

TEST_CASE("withWifiPassword returns a new config with the password changed and everything else unchanged")
{
    LocoNetAdapterConfig config("MyHomeWifi", "old-pass", "192.168.1.13", 1234);

    LocoNetAdapterConfig updated = config.withWifiPassword("new-pass");

    REQUIRE(updated.wifiSsid() == "MyHomeWifi");
    REQUIRE(updated.wifiPassword() == "new-pass");
    REQUIRE(updated.jmriHost() == "192.168.1.13");
    REQUIRE(updated.jmriPort() == 1234);
}

TEST_CASE("withJmriHost returns a new config with the JMRI host changed and everything else unchanged")
{
    LocoNetAdapterConfig config("MyHomeWifi", "hunter2", "Old", 1234);

    LocoNetAdapterConfig updated = config.withJmriHost("192.168.1.13");

    REQUIRE(updated.jmriHost() == "192.168.1.13");
    REQUIRE(updated.wifiSsid() == "MyHomeWifi");
    REQUIRE(updated.wifiPassword() == "hunter2");
    REQUIRE(updated.jmriPort() == 1234);
}

TEST_CASE("withJmriPort returns a new config with the JMRI port changed and everything else unchanged")
{
    LocoNetAdapterConfig config("MyHomeWifi", "hunter2", "192.168.1.13", 1111);

    LocoNetAdapterConfig updated = config.withJmriPort(1234);

    REQUIRE(updated.jmriPort() == 1234);
    REQUIRE(updated.wifiSsid() == "MyHomeWifi");
    REQUIRE(updated.wifiPassword() == "hunter2");
    REQUIRE(updated.jmriHost() == "192.168.1.13");
}

TEST_CASE("Configs with equal fields compare equal")
{
    LocoNetAdapterConfig a("MyHomeWifi", "hunter2", "192.168.1.13", 1234);
    LocoNetAdapterConfig b("MyHomeWifi", "hunter2", "192.168.1.13", 1234);

    REQUIRE(a == b);
}

TEST_CASE("Configs with different WiFi fields compare unequal")
{
    LocoNetAdapterConfig a("MyHomeWifi", "hunter2");
    LocoNetAdapterConfig b("MyHomeWifi", "different");

    REQUIRE(a != b);
}

TEST_CASE("Configs with different JMRI hosts compare unequal")
{
    LocoNetAdapterConfig a("MyHomeWifi", "hunter2", "192.168.1.13", 1234);
    LocoNetAdapterConfig b("MyHomeWifi", "hunter2", "192.168.1.14", 1234);

    REQUIRE(a != b);
}

TEST_CASE("Configs with different JMRI ports compare unequal")
{
    LocoNetAdapterConfig a("MyHomeWifi", "hunter2", "192.168.1.13", 1234);
    LocoNetAdapterConfig b("MyHomeWifi", "hunter2", "192.168.1.13", 4321);

    REQUIRE(a != b);
}
