#include "WebFormCommissioningAdapter.h"

#include "domain/SetupFormRenderer.h"

WebFormCommissioningAdapter::WebFormCommissioningAdapter(ConfigStore& configStore, RebootTrigger& rebootTrigger)
    : configStore_(configStore), rebootTrigger_(rebootTrigger)
{
}

std::string WebFormCommissioningAdapter::renderPage() const
{
    return renderSetupForm(configStore_.load());
}

bool WebFormCommissioningAdapter::wouldAccept(const std::string& ssid, const std::string& password) const
{
    return LocoNetAdapterConfig(ssid, password).isComplete();
}

void WebFormCommissioningAdapter::handleSubmission(const std::string& ssid, const std::string& password)
{
    if (!wouldAccept(ssid, password))
    {
        return;
    }
    configStore_.save(LocoNetAdapterConfig(ssid, password));
    rebootTrigger_.reboot();
}
