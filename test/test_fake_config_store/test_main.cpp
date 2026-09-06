#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/LocoNetAdapterConfig.h"
#include "support/FakeConfigStore.h"

TEST_CASE("load() returns a default-constructed config when nothing has been saved")
{
    FakeConfigStore store;

    LocoNetAdapterConfig config = store.load();

    REQUIRE(config == LocoNetAdapterConfig());
}

TEST_CASE("save() then load() returns the saved config")
{
    FakeConfigStore store;
    LocoNetAdapterConfig config("MyHomeWifi", "hunter2");

    store.save(config);

    REQUIRE(store.load() == config);
}

TEST_CASE("saveCount() tracks how many times save() was called")
{
    FakeConfigStore store;

    store.save(LocoNetAdapterConfig("A", "1"));
    store.save(LocoNetAdapterConfig("B", "2"));

    REQUIRE(store.saveCount() == 2);
}
