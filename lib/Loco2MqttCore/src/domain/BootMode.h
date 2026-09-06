#pragma once

#include "domain/LocoNetAdapterConfig.h"

enum class BootMode
{
    Normal,
    NeedsCommissioning,
    WirelessSetup
};

BootMode selectBootMode(const LocoNetAdapterConfig& config, bool setupModeRequested);
