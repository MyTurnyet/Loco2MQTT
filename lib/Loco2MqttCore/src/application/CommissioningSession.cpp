#include "CommissioningSession.h"

CommissioningSession::CommissioningSession(ConfigStore& configStore)
    : configStore_(configStore)
{
}

std::string CommissioningSession::apply(const ParsedCommand& command)
{
    switch (command.type())
    {
        case CommandType::SetSsid:
            return applySetSsid(command.value());
        case CommandType::SetPassword:
            return applySetPassword(command.value());
        case CommandType::SetJmriHost:
            return applySetJmriHost(command.value());
        case CommandType::SetJmriPort:
            return applySetJmriPort(command.value());
        case CommandType::Show:
            return applyShow();
        case CommandType::Save:
            return applySave();
        default:
            return "ERR unknown command";
    }
}

std::string CommissioningSession::applySetSsid(const std::string& value)
{
    pending_ = pending_.withWifiSsid(value);
    return "OK";
}

std::string CommissioningSession::applySetPassword(const std::string& value)
{
    pending_ = pending_.withWifiPassword(value);
    return "OK";
}

std::string CommissioningSession::applySetJmriHost(const std::string& value)
{
    pending_ = pending_.withJmriHost(value);
    return "OK";
}

std::string CommissioningSession::applySetJmriPort(const std::string& value)
{
    pending_ = pending_.withJmriPort(static_cast<uint16_t>(std::stoul(value)));
    return "OK";
}

std::string CommissioningSession::applyShow() const
{
    const std::string passwordState = pending_.wifiPassword().empty() ? "unset" : "set";
    const std::string portState = pending_.jmriPort() == 0 ? "unset" : std::to_string(pending_.jmriPort());
    return "ssid=" + pending_.wifiSsid() + " password=" + passwordState +
           " jmri_host=" + pending_.jmriHost() + " jmri_port=" + portState;
}

std::string CommissioningSession::applySave()
{
    configStore_.save(pending_);
    return "SAVED";
}
