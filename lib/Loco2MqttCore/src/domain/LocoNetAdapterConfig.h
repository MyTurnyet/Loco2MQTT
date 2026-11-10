#pragma once

#include <string>

class LocoNetAdapterConfig
{
public:
    LocoNetAdapterConfig() = default;
    LocoNetAdapterConfig(std::string wifiSsid, std::string wifiPassword);

    const std::string& wifiSsid() const;
    const std::string& wifiPassword() const;
    bool isComplete() const;

    LocoNetAdapterConfig withWifiSsid(const std::string& value) const;
    LocoNetAdapterConfig withWifiPassword(const std::string& value) const;

    bool operator==(const LocoNetAdapterConfig& other) const;
    bool operator!=(const LocoNetAdapterConfig& other) const;

private:
    std::string wifiSsid_;
    std::string wifiPassword_;
};
