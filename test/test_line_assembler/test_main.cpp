#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "domain/LineAssembler.h"

TEST_CASE("feeding characters with no newline yields nullopt")
{
    LineAssembler assembler(256);

    REQUIRE(assembler.feed('a') == std::nullopt);
    REQUIRE(assembler.feed('b') == std::nullopt);
}

TEST_CASE("a newline completes the line")
{
    LineAssembler assembler(256);
    assembler.feed('h');
    assembler.feed('i');

    REQUIRE(assembler.feed('\n') == std::optional<std::string>("hi"));
}

TEST_CASE("a trailing carriage return is stripped")
{
    LineAssembler assembler(256);
    assembler.feed('h');
    assembler.feed('i');
    assembler.feed('\r');

    REQUIRE(assembler.feed('\n') == std::optional<std::string>("hi"));
}

TEST_CASE("the buffer resets after a completed line, ready for the next one")
{
    LineAssembler assembler(256);
    assembler.feed('a');
    assembler.feed('\n');

    assembler.feed('b');
    REQUIRE(assembler.feed('\n') == std::optional<std::string>("b"));
}

TEST_CASE("bytes beyond maxBufferedBytes are dropped, not appended")
{
    LineAssembler assembler(3);
    assembler.feed('a');
    assembler.feed('b');
    assembler.feed('c');
    assembler.feed('d');

    REQUIRE(assembler.feed('\n') == std::optional<std::string>("abc"));
}

TEST_CASE("a custom terminator completes the line")
{
    LineAssembler assembler(256, '\r');
    assembler.feed('h');
    assembler.feed('i');

    REQUIRE(assembler.feed('\r') == std::optional<std::string>("hi"));
}

TEST_CASE("a custom terminator does not strip a trailing carriage return")
{
    LineAssembler assembler(256, '\r');
    assembler.feed('h');
    assembler.feed('i');
    assembler.feed('\r');

    REQUIRE(assembler.feed('\n') == std::nullopt);
}
