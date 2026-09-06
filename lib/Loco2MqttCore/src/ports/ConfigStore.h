#pragma once

#include "domain/LocoNetAdapterConfig.h"

class ConfigStore
{
public:
    virtual ~ConfigStore() = default;
    virtual LocoNetAdapterConfig load() = 0;
    virtual void save(const LocoNetAdapterConfig& config) = 0;
};
