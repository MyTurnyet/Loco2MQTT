#include "ParsedCommand.h"

ParsedCommand::ParsedCommand(CommandType type, std::string value)
    : type_(type), value_(std::move(value))
{
}

ParsedCommand ParsedCommand::setSsid(const std::string& value)
{
    return ParsedCommand(CommandType::SetSsid, value);
}

ParsedCommand ParsedCommand::setPassword(const std::string& value)
{
    return ParsedCommand(CommandType::SetPassword, value);
}

ParsedCommand ParsedCommand::show()
{
    return ParsedCommand(CommandType::Show, "");
}

ParsedCommand ParsedCommand::save()
{
    return ParsedCommand(CommandType::Save, "");
}

ParsedCommand ParsedCommand::unknown(const std::string& rawLine)
{
    return ParsedCommand(CommandType::Unknown, rawLine);
}

CommandType ParsedCommand::type() const
{
    return type_;
}

const std::string& ParsedCommand::value() const
{
    return value_;
}
