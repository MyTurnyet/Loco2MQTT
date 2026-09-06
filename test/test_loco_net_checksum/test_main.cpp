#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/LocoNetChecksum.h"

TEST_CASE("computeLocoNetChecksum produces a checksum that XORs the whole message to 0xFF")
{
    // 0xB0, 0x04, 0x30 (address 5, Closed, output on) -> checksum 0x7B,
    // hand-verified: 0xB0 ^ 0x04 ^ 0x30 ^ 0x7B == 0xFF
    std::vector<uint8_t> bytes = {0xB0, 0x04, 0x30};

    REQUIRE(computeLocoNetChecksum(bytes) == 0x7B);
}

TEST_CASE("computeLocoNetChecksum handles the off-pulse variant of the same message")
{
    // Same address/direction, output off (0x20 only) -> checksum 0x6B
    std::vector<uint8_t> bytes = {0xB0, 0x04, 0x20};

    REQUIRE(computeLocoNetChecksum(bytes) == 0x6B);
}

TEST_CASE("computeLocoNetChecksum handles the maximum turnout address")
{
    // Address 2048 (zero-based 2047 = 0x7FF), Closed, output on -> checksum 0x0F
    std::vector<uint8_t> bytes = {0xB0, 0x7F, 0x3F};

    REQUIRE(computeLocoNetChecksum(bytes) == 0x0F);
}
