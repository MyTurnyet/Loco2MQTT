#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/LocoNetMessage.h"

TEST_CASE("describe() renders an empty message as an empty string")
{
    LocoNetMessage message({});

    REQUIRE(message.describe() == "");
}

TEST_CASE("describe() renders a single byte as two uppercase hex digits")
{
    LocoNetMessage message({0x0A});

    REQUIRE(message.describe() == "0A");
}

TEST_CASE("describe() renders multiple bytes space-separated")
{
    LocoNetMessage message({0xB2, 0x00, 0x00, 0x50});

    REQUIRE(message.describe() == "B2 00 00 50");
}

TEST_CASE("bytes() returns the original bytes")
{
    LocoNetMessage message({0xB2, 0x00});

    REQUIRE(message.bytes() == std::vector<uint8_t>{0xB2, 0x00});
}

TEST_CASE("Messages with equal bytes compare equal")
{
    LocoNetMessage a({0xB2, 0x00});
    LocoNetMessage b({0xB2, 0x00});

    REQUIRE(a == b);
}

TEST_CASE("Messages with different bytes compare unequal")
{
    LocoNetMessage a({0xB2, 0x00});
    LocoNetMessage b({0xB2, 0x01});

    REQUIRE(a != b);
}
