#ifdef ARDUINO

#include "NvsConfigStore.h"

#include <Preferences.h>

namespace
{
    constexpr const char* kNamespace = "loco2mqtt";
    constexpr const char* kSsidKey = "ssid";
    constexpr const char* kPasswordKey = "password";
}

LocoNetAdapterConfig NvsConfigStore::load()
{
    Preferences preferences;
    preferences.begin(kNamespace, /* readOnly = */ true);
    const std::string ssid = preferences.getString(kSsidKey, "").c_str();
    const std::string password = preferences.getString(kPasswordKey, "").c_str();
    preferences.end();
    return LocoNetAdapterConfig(ssid, password);
}

void NvsConfigStore::save(const LocoNetAdapterConfig& config)
{
    Preferences preferences;
    preferences.begin(kNamespace, /* readOnly = */ false);
    preferences.putString(kSsidKey, config.wifiSsid().c_str());
    preferences.putString(kPasswordKey, config.wifiPassword().c_str());
    preferences.end();
}

#endif
