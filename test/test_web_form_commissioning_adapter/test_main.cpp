#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "adapters/WebFormCommissioningAdapter.h"
#include "domain/LocoNetAdapterConfig.h"
#include "support/FakeConfigStore.h"
#include "support/FakeRebootTrigger.h"

TEST_CASE("renderPage reflects the currently stored config")
{
    FakeConfigStore store;
    FakeRebootTrigger reboot;
    store.save(LocoNetAdapterConfig("MyHomeWifi", "hunter2"));
    WebFormCommissioningAdapter adapter(store, reboot);

    std::string page = adapter.renderPage();

    REQUIRE(page.find("value=\"MyHomeWifi\"") != std::string::npos);
}

TEST_CASE("a valid submission saves the config and reboots")
{
    FakeConfigStore store;
    FakeRebootTrigger reboot;
    WebFormCommissioningAdapter adapter(store, reboot);

    adapter.handleSubmission("MyHomeWifi", "hunter2");

    REQUIRE(store.load() == LocoNetAdapterConfig("MyHomeWifi", "hunter2"));
    REQUIRE(reboot.rebootCount() == 1);
}

TEST_CASE("a submission with an empty ssid is rejected without saving or rebooting")
{
    FakeConfigStore store;
    FakeRebootTrigger reboot;
    WebFormCommissioningAdapter adapter(store, reboot);

    adapter.handleSubmission("", "hunter2");

    REQUIRE(store.saveCount() == 0);
    REQUIRE(reboot.rebootCount() == 0);
}

TEST_CASE("a submission with an empty password is rejected without saving or rebooting")
{
    FakeConfigStore store;
    FakeRebootTrigger reboot;
    WebFormCommissioningAdapter adapter(store, reboot);

    adapter.handleSubmission("MyHomeWifi", "");

    REQUIRE(store.saveCount() == 0);
    REQUIRE(reboot.rebootCount() == 0);
}
