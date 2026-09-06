#include "LineAssembler.h"

LineAssembler::LineAssembler(std::size_t maxBufferedBytes)
    : maxBufferedBytes_(maxBufferedBytes)
{
}

std::optional<std::string> LineAssembler::feed(char c)
{
    if (c == '\n')
    {
        return finishLine();
    }
    if (buffer_.size() < maxBufferedBytes_)
    {
        buffer_ += c;
    }
    return std::nullopt;
}

std::string LineAssembler::finishLine()
{
    std::string line = buffer_;
    buffer_.clear();
    if (!line.empty() && line.back() == '\r')
    {
        line.pop_back();
    }
    return line;
}
