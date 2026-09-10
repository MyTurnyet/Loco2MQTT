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
    store.save(LocoNetAdapterConfig("MyHomeWifi", "hunter2", "192.168.1.13", 1234));
    WebFormCommissioningAdapter adapter(store, reboot);

    std::string page = adapter.renderPage();

    REQUIRE(page.find("value=\"MyHomeWifi\"") != std::string::npos);
    REQUIRE(page.find("value=\"192.168.1.13\"") != std::string::npos);
}

TEST_CASE("a valid submission saves the config and reboots")
{
    FakeConfigStore store;
    FakeRebootTrigger reboot;
    WebFormCommissioningAdapter adapter(store, reboot);

    adapter.handleSubmission("MyHomeWifi", "hunter2", "192.168.1.13", "1234");

    REQUIRE(store.load() == LocoNetAdapterConfig("MyHomeWifi", "hunter2", "192.168.1.13", 1234));
    REQUIRE(reboot.rebootCount() == 1);
}

TEST_CASE("a submission with an empty ssid is rejected without saving or rebooting")
{
    FakeConfigStore store;
    FakeRebootTrigger reboot;
    WebFormCommissioningAdapter adapter(store, reboot);

    adapter.handleSubmission("", "hunter2", "192.168.1.13", "1234");

    REQUIRE(store.saveCount() == 0);
    REQUIRE(reboot.rebootCount() == 0);
}

TEST_CASE("a submission with an empty password is rejected without saving or rebooting")
{
    FakeConfigStore store;
    FakeRebootTrigger reboot;
    WebFormCommissioningAdapter adapter(store, reboot);

    adapter.handleSubmission("MyHomeWifi", "", "192.168.1.13", "1234");

    REQUIRE(store.saveCount() == 0);
    REQUIRE(reboot.rebootCount() == 0);
}

TEST_CASE("a submission with an empty JMRI host is rejected without saving or rebooting")
{
    FakeConfigStore store;
    FakeRebootTrigger reboot;
    WebFormCommissioningAdapter adapter(store, reboot);

    adapter.handleSubmission("MyHomeWifi", "hunter2", "", "1234");

    REQUIRE(store.saveCount() == 0);
    REQUIRE(reboot.rebootCount() == 0);
}

TEST_CASE("a submission with a non-numeric JMRI port is rejected without saving or rebooting")
{
    FakeConfigStore store;
    FakeRebootTrigger reboot;
    WebFormCommissioningAdapter adapter(store, reboot);

    adapter.handleSubmission("MyHomeWifi", "hunter2", "192.168.1.13", "not-a-port");

    REQUIRE(store.saveCount() == 0);
    REQUIRE(reboot.rebootCount() == 0);
}

TEST_CASE("a submission with an out-of-range JMRI port is rejected without saving or rebooting")
{
    FakeConfigStore store;
    FakeRebootTrigger reboot;
    WebFormCommissioningAdapter adapter(store, reboot);

    adapter.handleSubmission("MyHomeWifi", "hunter2", "192.168.1.13", "70000");

    REQUIRE(store.saveCount() == 0);
    REQUIRE(reboot.rebootCount() == 0);
}

TEST_CASE("wouldAccept reports true only for a complete, valid submission")
{
    FakeConfigStore store;
    FakeRebootTrigger reboot;
    WebFormCommissioningAdapter adapter(store, reboot);

    REQUIRE(adapter.wouldAccept("MyHomeWifi", "hunter2", "192.168.1.13", "1234") == true);
    REQUIRE(adapter.wouldAccept("", "hunter2", "192.168.1.13", "1234") == false);
    REQUIRE(adapter.wouldAccept("MyHomeWifi", "", "192.168.1.13", "1234") == false);
    REQUIRE(adapter.wouldAccept("MyHomeWifi", "hunter2", "", "1234") == false);
    REQUIRE(adapter.wouldAccept("MyHomeWifi", "hunter2", "192.168.1.13", "") == false);
    REQUIRE(adapter.wouldAccept("MyHomeWifi", "hunter2", "192.168.1.13", "bogus") == false);
}
