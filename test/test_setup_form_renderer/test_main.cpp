#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

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
