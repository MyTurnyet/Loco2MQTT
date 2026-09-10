#include "WebFormCommissioningAdapter.h"

#include "domain/NetworkPortParser.h"
#include "domain/SetupFormRenderer.h"

WebFormCommissioningAdapter::WebFormCommissioningAdapter(ConfigStore& configStore, RebootTrigger& rebootTrigger)
    : configStore_(configStore), rebootTrigger_(rebootTrigger)
{
}

std::string WebFormCommissioningAdapter::renderPage() const
{
    return renderSetupForm(configStore_.load());
}

bool WebFormCommissioningAdapter::wouldAccept(const std::string& ssid, const std::string& password,
                                               const std::string& jmriHost, const std::string& jmriPort) const
{
    const auto port = parseNetworkPort(jmriPort);
    if (!port)
    {
        return false;
    }
    return LocoNetAdapterConfig(ssid, password, jmriHost, *port).isComplete();
}

void WebFormCommissioningAdapter::handleSubmission(const std::string& ssid, const std::string& password,
                                                     const std::string& jmriHost, const std::string& jmriPort)
{
    if (!wouldAccept(ssid, password, jmriHost, jmriPort))
    {
        return;
    }
    configStore_.save(LocoNetAdapterConfig(ssid, password, jmriHost, *parseNetworkPort(jmriPort)));
    rebootTrigger_.reboot();
}
