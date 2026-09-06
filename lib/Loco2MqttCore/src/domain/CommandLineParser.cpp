#include "CommandLineParser.h"

namespace
{
    std::string verbOf(const std::string& line)
    {
        return line.substr(0, line.find(' '));
    }

    std::string argumentOf(const std::string& line)
    {
        const auto spacePos = line.find(' ');
        return spacePos == std::string::npos ? "" : line.substr(spacePos + 1);
    }
}

ParsedCommand parseCommandLine(const std::string& line)
{
    const std::string verb = verbOf(line);
    const std::string argument = argumentOf(line);

    if (verb == "set-ssid" && !argument.empty())
    {
        return ParsedCommand::setSsid(argument);
    }
    if (verb == "set-password" && !argument.empty())
    {
        return ParsedCommand::setPassword(argument);
    }
    if (verb == "show" && argument.empty())
    {
        return ParsedCommand::show();
    }
    if (verb == "save" && argument.empty())
    {
        return ParsedCommand::save();
    }
    return ParsedCommand::unknown(line);
}
