#pragma once

#ifdef ARDUINO

#include <optional>
#include <string>

#include "domain/LineAssembler.h"
#include "ports/UartPort.h"

class EspUartPort final : public UartPort
{
public:
    EspUartPort();

    std::optional<std::string> readLine() override;
    void writeLine(const std::string& line) override;

private:
    // Bounds worst-case heap growth if a client sends bytes with no
    // newline; SerialCommissioningAdapter separately rejects any
    // completed line over its own kMaxLineLength.
    static constexpr std::size_t kMaxBufferedBytes = 256;

    LineAssembler assembler_;
};

#endif
