#pragma once

#include <cstddef>
#include <string>

#include "application/CommissioningSession.h"
#include "domain/ParsedCommand.h"
#include "ports/UartPort.h"

class SerialCommissioningAdapter
{
public:
    SerialCommissioningAdapter(UartPort& uart, CommissioningSession& session);

    void update();

private:
    ParsedCommand parseLine(const std::string& line) const;

    UartPort& uart_;
    CommissioningSession& session_;

    static constexpr std::size_t kMaxLineLength = 128;
};
