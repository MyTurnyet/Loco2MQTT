#pragma once

#include <cstddef>
#include <optional>
#include <string>

class LineAssembler
{
public:
    explicit LineAssembler(std::size_t maxBufferedBytes, char terminator = '\n');

    std::optional<std::string> feed(char c);

private:
    std::string finishLine();

    std::string buffer_;
    std::size_t maxBufferedBytes_;
    char terminator_;
};
