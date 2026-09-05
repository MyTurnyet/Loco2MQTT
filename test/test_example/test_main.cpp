#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

TEST_CASE("The native Catch2 test harness runs")
{
    REQUIRE(1 + 1 == 2);
}
