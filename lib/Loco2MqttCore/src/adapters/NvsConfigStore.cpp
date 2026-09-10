#ifdef ARDUINO

#include "NvsConfigStore.h"

#include <Preferences.h>

namespace
{
    constexpr const char* kNamespace = "loco2mqtt";
    constexpr const char* kSsidKey = "ssid";
    constexpr const char* kPasswordKey = "password";
    constexpr const char* kJmriHostKey = "jmri_host";
    constexpr const char* kJmriPortKey = "jmri_port";
}

LocoNetAdapterConfig NvsConfigStore::load()
{
    Preferences preferences;
    preferences.begin(kNamespace, /* readOnly = */ true);
    const std::string ssid = preferences.getString(kSsidKey, "").c_str();
    const std::string password = preferences.getString(kPasswordKey, "").c_str();
    const std::string jmriHost = preferences.getString(kJmriHostKey, "").c_str();
    const uint16_t jmriPort = preferences.getUShort(kJmriPortKey, 0);
    preferences.end();
    return LocoNetAdapterConfig(ssid, password, jmriHost, jmriPort);
}

void NvsConfigStore::save(const LocoNetAdapterConfig& config)
{
    Preferences preferences;
    preferences.begin(kNamespace, /* readOnly = */ false);
    preferences.putString(kSsidKey, config.wifiSsid().c_str());
    preferences.putString(kPasswordKey, config.wifiPassword().c_str());
    preferences.putString(kJmriHostKey, config.jmriHost().c_str());
    preferences.putUShort(kJmriPortKey, config.jmriPort());
    preferences.end();
}

#endif
