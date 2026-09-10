#pragma once

#include <string>

enum class CommandType
{
    SetSsid,
    SetPassword,
    SetJmriHost,
    SetJmriPort,
    Show,
    Save,
    Unknown
};

class ParsedCommand
{
public:
    static ParsedCommand setSsid(const std::string& value);
    static ParsedCommand setPassword(const std::string& value);
    static ParsedCommand setJmriHost(const std::string& value);
    static ParsedCommand setJmriPort(const std::string& value);
    static ParsedCommand show();
    static ParsedCommand save();
    static ParsedCommand unknown(const std::string& rawLine);

    CommandType type() const;
    const std::string& value() const;

private:
    ParsedCommand(CommandType type, std::string value);

    CommandType type_;
    std::string value_;
};
