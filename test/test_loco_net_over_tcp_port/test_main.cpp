#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "adapters/LocoNetOverTcpPort.h"
#include "support/FakeLineStream.h"

TEST_CASE("send() writes the SEND-encoded line to the stream")
{
    FakeLineStream stream;
    LocoNetOverTcpPort port(stream);

    port.send(LocoNetMessage({0xBB, 0x01, 0x00, 0x45}));

    REQUIRE(stream.writtenLines().size() == 1);
    REQUIRE(stream.writtenLines()[0] == "SEND BB 01 00 45");
}

TEST_CASE("receive() returns nullopt when the stream has no lines")
{
    FakeLineStream stream;
    LocoNetOverTcpPort port(stream);

    REQUIRE(port.receive() == std::nullopt);
}

TEST_CASE("receive() decodes a RECEIVE line into a message")
{
    FakeLineStream stream;
    stream.enqueueLine("RECEIVE BB 01 00 45");
    LocoNetOverTcpPort port(stream);

    REQUIRE(port.receive() == LocoNetMessage({0xBB, 0x01, 0x00, 0x45}));
}

TEST_CASE("receive() transparently skips one non-data line ahead of a real message")
{
    FakeLineStream stream;
    stream.enqueueLine("VERSION JMRI Server 5.2+R760b98537f");
    stream.enqueueLine("RECEIVE BB 01 00 45");
    LocoNetOverTcpPort port(stream);

    REQUIRE(port.receive() == LocoNetMessage({0xBB, 0x01, 0x00, 0x45}));
}

TEST_CASE("receive() transparently skips several non-data lines ahead of a real message")
{
    FakeLineStream stream;
    stream.enqueueLine("VERSION JMRI Server 5.2+R760b98537f");
    stream.enqueueLine("SENT OK");
    stream.enqueueLine("RECEIVE E7 0E 01 33");
    LocoNetOverTcpPort port(stream);

    REQUIRE(port.receive() == LocoNetMessage({0xE7, 0x0E, 0x01, 0x33}));
}

TEST_CASE("receive() returns nullopt once only non-data lines remain")
{
    FakeLineStream stream;
    stream.enqueueLine("SENT OK");
    LocoNetOverTcpPort port(stream);

    REQUIRE(port.receive() == std::nullopt);
}

TEST_CASE("receive() returns nullopt when the stream reports disconnected and has nothing enqueued")
{
    FakeLineStream stream;
    stream.setConnected(false);
    LocoNetOverTcpPort port(stream);

    REQUIRE(port.receive() == std::nullopt);
}

TEST_CASE("send() still writes to the stream even when the stream reports disconnected")
{
    // LocoNetOverTcpPort deliberately never inspects isConnected() itself —
    // that's LineStream's own responsibility (a real WiFiClientLineStream
    // no-ops writeLine()/returns nullopt from readLine() while down). The
    // port just delegates.
    FakeLineStream stream;
    stream.setConnected(false);
    LocoNetOverTcpPort port(stream);

    port.send(LocoNetMessage({0xAA}));

    REQUIRE(stream.writtenLines().size() == 1);
}
