#pragma once

#include <cstdint>
#include <string>

class LocoNetAdapterConfig
{
public:
    LocoNetAdapterConfig() = default;
    LocoNetAdapterConfig(std::string wifiSsid, std::string wifiPassword,
                          std::string jmriHost = "", uint16_t jmriPort = 0);

    const std::string& wifiSsid() const;
    const std::string& wifiPassword() const;
    const std::string& jmriHost() const;
    uint16_t jmriPort() const;
    bool isComplete() const;

    LocoNetAdapterConfig withWifiSsid(const std::string& value) const;
    LocoNetAdapterConfig withWifiPassword(const std::string& value) const;
    LocoNetAdapterConfig withJmriHost(const std::string& value) const;
    LocoNetAdapterConfig withJmriPort(uint16_t value) const;

    bool operator==(const LocoNetAdapterConfig& other) const;
    bool operator!=(const LocoNetAdapterConfig& other) const;

private:
    std::string wifiSsid_;
    std::string wifiPassword_;
    std::string jmriHost_;
    uint16_t jmriPort_ = 0;
};
