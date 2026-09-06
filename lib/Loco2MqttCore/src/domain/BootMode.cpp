#include "BootMode.h"

BootMode selectBootMode(const LocoNetAdapterConfig& config, bool setupModeRequested)
{
    if (setupModeRequested)
    {
        return BootMode::WirelessSetup;
    }
    if (!config.isComplete())
    {
        return BootMode::NeedsCommissioning;
    }
    return BootMode::Normal;
}
