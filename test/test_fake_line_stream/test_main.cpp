#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "support/FakeLineStream.h"

TEST_CASE("readLine() returns nullopt when nothing was enqueued")
{
    FakeLineStream stream;

    REQUIRE(stream.readLine() == std::nullopt);
}

TEST_CASE("readLine() returns an enqueued line")
{
    FakeLineStream stream;
    stream.enqueueLine("RECEIVE 83 7C");

    REQUIRE(stream.readLine() == "RECEIVE 83 7C");
}

TEST_CASE("readLine() returns enqueued lines in FIFO order, then nullopt")
{
    FakeLineStream stream;
    stream.enqueueLine("VERSION LbServer 0.1a");
    stream.enqueueLine("RECEIVE 83 7C");

    REQUIRE(stream.readLine() == "VERSION LbServer 0.1a");
    REQUIRE(stream.readLine() == "RECEIVE 83 7C");
    REQUIRE(stream.readLine() == std::nullopt);
}

TEST_CASE("writeLine() records the written line")
{
    FakeLineStream stream;

    stream.writeLine("SEND A0 2F 00 70");

    REQUIRE(stream.writtenLines().size() == 1);
    REQUIRE(stream.writtenLines()[0] == "SEND A0 2F 00 70");
}

TEST_CASE("writeLine() records multiple lines in order")
{
    FakeLineStream stream;

    stream.writeLine("SEND A0 2F 00 70");
    stream.writeLine("SEND B2 00 00 50");

    REQUIRE(stream.writtenLines().size() == 2);
    REQUIRE(stream.writtenLines()[0] == "SEND A0 2F 00 70");
    REQUIRE(stream.writtenLines()[1] == "SEND B2 00 00 50");
}

TEST_CASE("A fresh FakeLineStream reports connected")
{
    FakeLineStream stream;

    REQUIRE(stream.isConnected());
}

TEST_CASE("setConnected(false) makes isConnected() report false")
{
    FakeLineStream stream;

    stream.setConnected(false);

    REQUIRE_FALSE(stream.isConnected());
}

TEST_CASE("setConnected(true) after setConnected(false) reports connected again")
{
    FakeLineStream stream;
    stream.setConnected(false);

    stream.setConnected(true);

    REQUIRE(stream.isConnected());
}
