#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "adapters/LocoNetOverTcpCodec.h"
#include "domain/LocoNetMessage.h"

// encodeSend() is TDD'd against the documented LocoNetOverTcp wire protocol
// (https://loconetovertcp.sourceforge.net/Protocol/LoconetOverTcp.html):
// "SEND <byte sequence>", bytes hex-encoded and space-separated, e.g.
// "SEND A0 2F 00 70".
//
// decodeLine() is TDD'd against lines actually captured from a live JMRI
// 5.2 "Start LocoNet Server" session (netcat, 2026-09-10):
//   RECV: b'VERSION JMRI Server 5.2+R760b98537f\r'
//   (SEND BB 01 00 45 sent to the server)
//   RECV: b'RECEIVE BB 01 00 45\r'
//   RECV: b'SENT OK\r'
//   RECV: b'RECEIVE E7 0E 01 33 0F 00 00 07 08 4E 00 35 44 1B\r'
// Lines below are given without the trailing \r: framing/terminator
// stripping is LineStream's job (WiFiClientLineStream), same contract as
// UartPort/LineAssembler — decodeLine() only ever sees a already-split line.

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

TEST_CASE("decodeLine() decodes a real captured RECEIVE line into its bytes")
{
    LocoNetOverTcpCodec codec;

    REQUIRE(codec.decodeLine("RECEIVE BB 01 00 45") == LocoNetMessage({0xBB, 0x01, 0x00, 0x45}));
}

TEST_CASE("decodeLine() decodes a longer real captured RECEIVE line")
{
    LocoNetOverTcpCodec codec;

    auto decoded = codec.decodeLine("RECEIVE E7 0E 01 33 0F 00 00 07 08 4E 00 35 44 1B");

    REQUIRE(decoded == LocoNetMessage({0xE7, 0x0E, 0x01, 0x33, 0x0F, 0x00, 0x00, 0x07, 0x08, 0x4E, 0x00, 0x35, 0x44, 0x1B}));
}

TEST_CASE("decodeLine() ignores the VERSION greeting line")
{
    LocoNetOverTcpCodec codec;

    REQUIRE(codec.decodeLine("VERSION JMRI Server 5.2+R760b98537f") == std::nullopt);
}

TEST_CASE("decodeLine() ignores a SENT OK confirmation line")
{
    LocoNetOverTcpCodec codec;

    REQUIRE(codec.decodeLine("SENT OK") == std::nullopt);
}

TEST_CASE("decodeLine() ignores a SENT ERROR confirmation line")
{
    LocoNetOverTcpCodec codec;

    REQUIRE(codec.decodeLine("SENT ERROR Did not receive echo from LocoBuffer") == std::nullopt);
}

TEST_CASE("decodeLine() ignores an empty line")
{
    LocoNetOverTcpCodec codec;

    REQUIRE(codec.decodeLine("") == std::nullopt);
}

TEST_CASE("decodeLine() ignores a RECEIVE line with a malformed byte token")
{
    LocoNetOverTcpCodec codec;

    REQUIRE(codec.decodeLine("RECEIVE BB ZZ 00 45") == std::nullopt);
}

TEST_CASE("decodeLine() ignores a RECEIVE line with no bytes at all")
{
    LocoNetOverTcpCodec codec;

    REQUIRE(codec.decodeLine("RECEIVE") == std::nullopt);
}
