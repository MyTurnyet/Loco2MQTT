#pragma once

#include <string>

#include "domain/LocoNetAdapterConfig.h"
#include "domain/ParsedCommand.h"
#include "ports/ConfigStore.h"

class CommissioningSession
{
public:
    explicit CommissioningSession(ConfigStore& configStore);

    std::string apply(const ParsedCommand& command);

private:
    std::string applySetSsid(const std::string& value);
    std::string applySetPassword(const std::string& value);
    std::string applySetJmriHost(const std::string& value);
    std::string applySetJmriPort(const std::string& value);
    std::string applyShow() const;
    std::string applySave();

    ConfigStore& configStore_;
    LocoNetAdapterConfig pending_;
};
