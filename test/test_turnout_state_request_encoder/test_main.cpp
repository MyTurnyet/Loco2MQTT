#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/TurnoutAddress.h"
#include "turnout/TurnoutStateRequestEncoder.h"

TEST_CASE("encodes an OPC_SW_STATE request for address 1")
{
    TurnoutStateRequestEncoder encoder;

    const LocoNetMessage message = encoder.encode(TurnoutAddress(1));

    REQUIRE(message.bytes() == std::vector<uint8_t>{0xBC, 0x00, 0x00, 0x43});
}

TEST_CASE("encodes an OPC_SW_STATE request for address 5")
{
    TurnoutStateRequestEncoder encoder;

    const LocoNetMessage message = encoder.encode(TurnoutAddress(5));

    REQUIRE(message.bytes() == std::vector<uint8_t>{0xBC, 0x04, 0x00, 0x47});
}

TEST_CASE("encodes an OPC_SW_STATE request for address 48")
{
    TurnoutStateRequestEncoder encoder;

    const LocoNetMessage message = encoder.encode(TurnoutAddress(48));

    REQUIRE(message.bytes() == std::vector<uint8_t>{0xBC, 0x2F, 0x00, 0x6C});
}

TEST_CASE("encodes the maximum address, 2048")
{
    TurnoutStateRequestEncoder encoder;

    const LocoNetMessage message = encoder.encode(TurnoutAddress(2048));

    REQUIRE(message.bytes() == std::vector<uint8_t>{0xBC, 0x7F, 0x0F, 0x33});
}
