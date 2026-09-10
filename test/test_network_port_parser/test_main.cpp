#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/NetworkPortParser.h"

TEST_CASE("parses a plain numeric string")
{
    REQUIRE(parseNetworkPort("1234") == 1234);
}

TEST_CASE("parses the minimum valid port")
{
    REQUIRE(parseNetworkPort("1") == 1);
}

TEST_CASE("parses the maximum valid port")
{
    REQUIRE(parseNetworkPort("65535") == 65535);
}

TEST_CASE("rejects zero")
{
    REQUIRE(parseNetworkPort("0") == std::nullopt);
}

TEST_CASE("rejects a value above 65535")
{
    REQUIRE(parseNetworkPort("65536") == std::nullopt);
}

TEST_CASE("rejects an empty string")
{
    REQUIRE(parseNetworkPort("") == std::nullopt);
}

TEST_CASE("rejects non-numeric text")
{
    REQUIRE(parseNetworkPort("abc") == std::nullopt);
}

TEST_CASE("rejects a numeric string with trailing garbage")
{
    REQUIRE(parseNetworkPort("1234x") == std::nullopt);
}

TEST_CASE("rejects a negative number")
{
    REQUIRE(parseNetworkPort("-1") == std::nullopt);
}
