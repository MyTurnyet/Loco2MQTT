#pragma once

#include <string>

#include "domain/LocoNetAdapterConfig.h"
#include "ports/ConfigStore.h"
#include "ports/RebootTrigger.h"

class WebFormCommissioningAdapter
{
public:
    WebFormCommissioningAdapter(ConfigStore& configStore, RebootTrigger& rebootTrigger);

    std::string renderPage() const;
    bool wouldAccept(const std::string& ssid, const std::string& password,
                      const std::string& jmriHost, const std::string& jmriPort) const;
    void handleSubmission(const std::string& ssid, const std::string& password,
                           const std::string& jmriHost, const std::string& jmriPort);

private:
    ConfigStore& configStore_;
    RebootTrigger& rebootTrigger_;
};
