#include "LocoNetAdapterConfig.h"

LocoNetAdapterConfig::LocoNetAdapterConfig(std::string wifiSsid, std::string wifiPassword,
                                            std::string jmriHost, uint16_t jmriPort)
    : wifiSsid_(std::move(wifiSsid)), wifiPassword_(std::move(wifiPassword)),
      jmriHost_(std::move(jmriHost)), jmriPort_(jmriPort)
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

const std::string& LocoNetAdapterConfig::jmriHost() const
{
    return jmriHost_;
}

uint16_t LocoNetAdapterConfig::jmriPort() const
{
    return jmriPort_;
}

bool LocoNetAdapterConfig::isComplete() const
{
    return !wifiSsid_.empty() && !wifiPassword_.empty()
        && !jmriHost_.empty() && jmriPort_ != 0;
}

LocoNetAdapterConfig LocoNetAdapterConfig::withWifiSsid(const std::string& value) const
{
    return LocoNetAdapterConfig(value, wifiPassword_, jmriHost_, jmriPort_);
}

LocoNetAdapterConfig LocoNetAdapterConfig::withWifiPassword(const std::string& value) const
{
    return LocoNetAdapterConfig(wifiSsid_, value, jmriHost_, jmriPort_);
}

LocoNetAdapterConfig LocoNetAdapterConfig::withJmriHost(const std::string& value) const
{
    return LocoNetAdapterConfig(wifiSsid_, wifiPassword_, value, jmriPort_);
}

LocoNetAdapterConfig LocoNetAdapterConfig::withJmriPort(uint16_t value) const
{
    return LocoNetAdapterConfig(wifiSsid_, wifiPassword_, jmriHost_, value);
}

bool LocoNetAdapterConfig::operator==(const LocoNetAdapterConfig& other) const
{
    return wifiSsid_ == other.wifiSsid_ && wifiPassword_ == other.wifiPassword_
        && jmriHost_ == other.jmriHost_ && jmriPort_ == other.jmriPort_;
}

bool LocoNetAdapterConfig::operator!=(const LocoNetAdapterConfig& other) const
{
    return !(*this == other);
}
