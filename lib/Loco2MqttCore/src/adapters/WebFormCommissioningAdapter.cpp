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

void WebFormCommissioningAdapter::handleSubmission(const std::string& ssid, const std::string& password)
{
    const LocoNetAdapterConfig config(ssid, password);
    if (!config.isComplete())
    {
        return;
    }
    configStore_.save(config);
    rebootTrigger_.reboot();
}
