#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "application/CommissioningSession.h"
#include "domain/CommandLineParser.h"
#include "domain/LocoNetAdapterConfig.h"
#include "domain/ParsedCommand.h"
#include "support/FakeConfigStore.h"

TEST_CASE("set-ssid returns OK and is reflected by a later show")
{
    FakeConfigStore store;
    CommissioningSession session(store);

    std::string reply = session.apply(parseCommandLine("set-ssid MyHomeWifi"));

    REQUIRE(reply == "OK");
    REQUIRE(session.apply(parseCommandLine("show")) ==
            "ssid=MyHomeWifi password=unset jmri_host= jmri_port=unset");
}

TEST_CASE("set-password returns OK and show never reveals the password value")
{
    FakeConfigStore store;
    CommissioningSession session(store);

    std::string reply = session.apply(parseCommandLine("set-password hunter2"));

    REQUIRE(reply == "OK");
    std::string shown = session.apply(parseCommandLine("show"));
    REQUIRE(shown == "ssid= password=set jmri_host= jmri_port=unset");
    REQUIRE(shown.find("hunter2") == std::string::npos);
}

TEST_CASE("show with nothing set reports an empty ssid, an unset password, and no JMRI host/port")
{
    FakeConfigStore store;
    CommissioningSession session(store);

    REQUIRE(session.apply(parseCommandLine("show")) ==
            "ssid= password=unset jmri_host= jmri_port=unset");
}

TEST_CASE("set-jmri-host returns OK and is reflected by a later show")
{
    FakeConfigStore store;
    CommissioningSession session(store);

    std::string reply = session.apply(parseCommandLine("set-jmri-host 192.168.1.13"));

    REQUIRE(reply == "OK");
    REQUIRE(session.apply(parseCommandLine("show")) ==
            "ssid= password=unset jmri_host=192.168.1.13 jmri_port=unset");
}

TEST_CASE("set-jmri-port returns OK and is reflected by a later show")
{
    FakeConfigStore store;
    CommissioningSession session(store);

    std::string reply = session.apply(parseCommandLine("set-jmri-port 1234"));

    REQUIRE(reply == "OK");
    REQUIRE(session.apply(parseCommandLine("show")) ==
            "ssid= password=unset jmri_host= jmri_port=1234");
}

TEST_CASE("setting fields does not save until an explicit save command")
{
    FakeConfigStore store;
    CommissioningSession session(store);

    session.apply(parseCommandLine("set-ssid MyHomeWifi"));
    session.apply(parseCommandLine("set-password hunter2"));

    REQUIRE(store.saveCount() == 0);
}

TEST_CASE("save persists the pending config and returns SAVED")
{
    FakeConfigStore store;
    CommissioningSession session(store);
    session.apply(parseCommandLine("set-ssid MyHomeWifi"));
    session.apply(parseCommandLine("set-password hunter2"));

    std::string reply = session.apply(parseCommandLine("save"));

    REQUIRE(reply == "SAVED");
    REQUIRE(store.saveCount() == 1);
    REQUIRE(store.load() == LocoNetAdapterConfig("MyHomeWifi", "hunter2"));
}

TEST_CASE("save persists JMRI host and port alongside WiFi fields")
{
    FakeConfigStore store;
    CommissioningSession session(store);
    session.apply(parseCommandLine("set-ssid MyHomeWifi"));
    session.apply(parseCommandLine("set-password hunter2"));
    session.apply(parseCommandLine("set-jmri-host 192.168.1.13"));
    session.apply(parseCommandLine("set-jmri-port 1234"));

    std::string reply = session.apply(parseCommandLine("save"));

    REQUIRE(reply == "SAVED");
    REQUIRE(store.load() == LocoNetAdapterConfig("MyHomeWifi", "hunter2", "192.168.1.13", 1234));
}

TEST_CASE("an unknown command reports a generic error and does not save")
{
    FakeConfigStore store;
    CommissioningSession session(store);

    std::string reply = session.apply(parseCommandLine("garbage"));

    REQUIRE(reply == "ERR unknown command");
    REQUIRE(store.saveCount() == 0);
}

TEST_CASE("an unknown command never echoes its raw input, even if it contains a password-like argument")
{
    FakeConfigStore store;
    CommissioningSession session(store);

    std::string reply = session.apply(parseCommandLine("set-passwrod hunter2"));

    REQUIRE(reply.find("hunter2") == std::string::npos);
}
