#pragma once

#ifdef ARDUINO

#include "ports/ConfigStore.h"

class NvsConfigStore final : public ConfigStore
{
public:
    LocoNetAdapterConfig load() override;
    void save(const LocoNetAdapterConfig& config) override;
};

#endif
