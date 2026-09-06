#ifdef ARDUINO

#include "NvsSetupModeRequestStore.h"

#include <Preferences.h>

namespace
{
    constexpr const char* kNamespace = "loco2mqtt";
    constexpr const char* kRequestedKey = "setup_req";
}

void NvsSetupModeRequestStore::request()
{
    Preferences preferences;
    preferences.begin(kNamespace, /* readOnly = */ false);
    preferences.putBool(kRequestedKey, true);
    preferences.end();
}

bool NvsSetupModeRequestStore::consumeIfRequested()
{
    Preferences preferences;
    preferences.begin(kNamespace, /* readOnly = */ false);
    const bool wasRequested = preferences.getBool(kRequestedKey, false);
    preferences.putBool(kRequestedKey, false);
    preferences.end();
    return wasRequested;
}

#endif
