#pragma once

#include <optional>
#include <string>

class UartPort
{
public:
    virtual ~UartPort() = default;
    virtual std::optional<std::string> readLine() = 0;
    virtual void writeLine(const std::string& line) = 0;
};
