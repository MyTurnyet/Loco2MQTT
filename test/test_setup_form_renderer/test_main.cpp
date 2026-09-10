#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/FirmwareVersion.h"
#include "domain/LocoNetAdapterConfig.h"
#include "domain/SetupFormRenderer.h"

TEST_CASE("renders the current SSID into the form")
{
    LocoNetAdapterConfig config("MyHomeWifi", "hunter2");

    std::string page = renderSetupForm(config);

    REQUIRE(page.find("value=\"MyHomeWifi\"") != std::string::npos);
}

TEST_CASE("never reflects the stored password back into the form")
{
    LocoNetAdapterConfig config("MyHomeWifi", "hunter2");

    std::string page = renderSetupForm(config);

    REQUIRE(page.find("hunter2") == std::string::npos);
    REQUIRE(page.find("name=\"password\"") != std::string::npos);
}

TEST_CASE("escapes special characters in the SSID so the HTML stays well-formed")
{
    LocoNetAdapterConfig config("My\"Wifi&Net", "hunter2");

    std::string page = renderSetupForm(config);

    REQUIRE(page.find("My\"Wifi&Net") == std::string::npos);
    REQUIRE(page.find("&quot;") != std::string::npos);
    REQUIRE(page.find("&amp;") != std::string::npos);
}

TEST_CASE("shows the firmware version on the page")
{
    LocoNetAdapterConfig config("MyHomeWifi", "hunter2");

    std::string page = renderSetupForm(config);

    REQUIRE(page.find(kFirmwareVersion) != std::string::npos);
}

TEST_CASE("renders the current JMRI host and port into the form")
{
    LocoNetAdapterConfig config("MyHomeWifi", "hunter2", "192.168.1.13", 1234);

    std::string page = renderSetupForm(config);

    REQUIRE(page.find("value=\"192.168.1.13\"") != std::string::npos);
    REQUIRE(page.find("value=\"1234\"") != std::string::npos);
}

TEST_CASE("leaves the JMRI port field blank when unset, rather than showing 0")
{
    LocoNetAdapterConfig config("MyHomeWifi", "hunter2");

    std::string page = renderSetupForm(config);

    REQUIRE(page.find("name=\"jmri_port\"") != std::string::npos);
    REQUIRE(page.find("value=\"0\"") == std::string::npos);
}

TEST_CASE("escapes special characters in the JMRI host so the HTML stays well-formed")
{
    LocoNetAdapterConfig config("MyHomeWifi", "hunter2", "host\"with&chars", 1234);

    std::string page = renderSetupForm(config);

    REQUIRE(page.find("host\"with&chars") == std::string::npos);
    REQUIRE(page.find("&quot;") != std::string::npos);
    REQUIRE(page.find("&amp;") != std::string::npos);
}
