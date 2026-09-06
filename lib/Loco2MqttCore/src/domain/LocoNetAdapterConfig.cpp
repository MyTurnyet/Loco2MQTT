#include "LocoNetAdapterConfig.h"

LocoNetAdapterConfig::LocoNetAdapterConfig(std::string wifiSsid, std::string wifiPassword)
    : wifiSsid_(std::move(wifiSsid)), wifiPassword_(std::move(wifiPassword))
{
}

const std::string& LocoNetAdapterConfig::wifiSsid() const
{
    return wifiSsid_;
}

const std::string& LocoNetAdapterConfig::wifiPassword() const
{
    return wifiPassword_;
}

bool LocoNetAdapterConfig::isComplete() const
{
    return !wifiSsid_.empty() && !wifiPassword_.empty();
}

LocoNetAdapterConfig LocoNetAdapterConfig::withWifiSsid(const std::string& value) const
{
    return LocoNetAdapterConfig(value, wifiPassword_);
}

LocoNetAdapterConfig LocoNetAdapterConfig::withWifiPassword(const std::string& value) const
{
    return LocoNetAdapterConfig(wifiSsid_, value);
}

bool LocoNetAdapterConfig::operator==(const LocoNetAdapterConfig& other) const
{
    return wifiSsid_ == other.wifiSsid_ && wifiPassword_ == other.wifiPassword_;
}

bool LocoNetAdapterConfig::operator!=(const LocoNetAdapterConfig& other) const
{
    return !(*this == other);
}
