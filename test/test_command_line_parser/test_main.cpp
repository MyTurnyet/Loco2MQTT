#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/CommandLineParser.h"
#include "domain/ParsedCommand.h"

TEST_CASE("parses set-ssid with its value")
{
    ParsedCommand command = parseCommandLine("set-ssid MyHomeWifi");

    REQUIRE(command.type() == CommandType::SetSsid);
    REQUIRE(command.value() == "MyHomeWifi");
}

TEST_CASE("parses set-password with its value")
{
    ParsedCommand command = parseCommandLine("set-password hunter2");

    REQUIRE(command.type() == CommandType::SetPassword);
    REQUIRE(command.value() == "hunter2");
}

TEST_CASE("parses show with no argument")
{
    ParsedCommand command = parseCommandLine("show");

    REQUIRE(command.type() == CommandType::Show);
}

TEST_CASE("parses save with no argument")
{
    ParsedCommand command = parseCommandLine("save");

    REQUIRE(command.type() == CommandType::Save);
}

TEST_CASE("an empty line is unknown")
{
    ParsedCommand command = parseCommandLine("");

    REQUIRE(command.type() == CommandType::Unknown);
    REQUIRE(command.value() == "");
}

TEST_CASE("an unrecognized verb is unknown")
{
    ParsedCommand command = parseCommandLine("bogus");

    REQUIRE(command.type() == CommandType::Unknown);
    REQUIRE(command.value() == "bogus");
}

TEST_CASE("set-ssid with no argument is unknown")
{
    ParsedCommand command = parseCommandLine("set-ssid");

    REQUIRE(command.type() == CommandType::Unknown);
    REQUIRE(command.value() == "set-ssid");
}

TEST_CASE("show with an unexpected argument is unknown")
{
    ParsedCommand command = parseCommandLine("show extra");

    REQUIRE(command.type() == CommandType::Unknown);
    REQUIRE(command.value() == "show extra");
}
