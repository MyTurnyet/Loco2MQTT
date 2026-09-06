#pragma once

#include <queue>
#include <string>
#include <vector>

#include "ports/UartPort.h"

class FakeUartPort : public UartPort
{
public:
    void enqueueLine(const std::string& line)
    {
        inbox_.push(line);
    }

    std::optional<std::string> readLine() override
    {
        if (inbox_.empty())
        {
            return std::nullopt;
        }
        const std::string line = inbox_.front();
        inbox_.pop();
        return line;
    }

    void writeLine(const std::string& line) override
    {
        outbox_.push_back(line);
    }

    const std::vector<std::string>& writtenLines() const
    {
        return outbox_;
    }

private:
    std::queue<std::string> inbox_;
    std::vector<std::string> outbox_;
};
