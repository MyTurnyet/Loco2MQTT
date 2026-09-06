#pragma once

#include "ports/ConfigStore.h"

class FakeConfigStore : public ConfigStore
{
public:
    LocoNetAdapterConfig load() override
    {
        return saved_;
    }

    void save(const LocoNetAdapterConfig& config) override
    {
        saved_ = config;
        saveCount_++;
    }

    int saveCount() const
    {
        return saveCount_;
    }

private:
    LocoNetAdapterConfig saved_;
    int saveCount_ = 0;
};
