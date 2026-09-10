#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "adapters/LocoNetOverTcpCodec.h"
#include "domain/LocoNetMessage.h"

// encodeSend() is TDD'd against the documented LocoNetOverTcp wire protocol
// (https://loconetovertcp.sourceforge.net/Protocol/LoconetOverTcp.html):
// "SEND <byte sequence>", bytes hex-encoded and space-separated, e.g.
// "SEND A0 2F 00 70". decodeLine() is deliberately not yet implemented —
// see docs/decisions/0001-interim-jmri-loconet-over-tcp-transport.md: the
// inbound line format is being confirmed empirically against a real running
// JMRI instance before any decode test is written, per that ADR's own
// instruction not to guess it from the protocol doc alone.

TEST_CASE("encodeSend() renders the protocol doc's own worked example")
{
    LocoNetOverTcpCodec codec;
    LocoNetMessage message({0xA0, 0x2F, 0x00, 0x70});

    REQUIRE(codec.encodeSend(message) == "SEND A0 2F 00 70");
}

TEST_CASE("encodeSend() renders a single-byte message")
{
    LocoNetOverTcpCodec codec;
    LocoNetMessage message({0xAA});

    REQUIRE(codec.encodeSend(message) == "SEND AA");
}

TEST_CASE("encodeSend() renders a two-byte message")
{
    LocoNetOverTcpCodec codec;
    LocoNetMessage message({0x83, 0x7C});

    REQUIRE(codec.encodeSend(message) == "SEND 83 7C");
}
