#include "SerialCommissioningAdapter.h"

#include "domain/CommandLineParser.h"

SerialCommissioningAdapter::SerialCommissioningAdapter(UartPort& uart, CommissioningSession& session)
    : uart_(uart), session_(session)
{
}

void SerialCommissioningAdapter::update()
{
    const std::optional<std::string> line = uart_.readLine();
    if (!line.has_value())
    {
        return;
    }
    const ParsedCommand command = parseLine(*line);
    uart_.writeLine(session_.apply(command));
}

ParsedCommand SerialCommissioningAdapter::parseLine(const std::string& line) const
{
    if (line.size() > kMaxLineLength)
    {
        return ParsedCommand::unknown(line);
    }
    return parseCommandLine(line);
}
