#pragma once

#ifdef ARDUINO

#include <string>

class EspMdnsPort
{
public:
    // Returns false on failure (e.g. mdns_init() or mdns_hostname_set()
    // failing internally) so the caller can retry on a later tick instead
    // of latching a permanent false success. Retries are throttled
    // internally (kRetryIntervalMs) so a persistent failure doesn't call
    // the vendor API -- and its internal log_e() error logging -- on every
    // loop() tick; mirrors EspWifiPort's own retry-interval pattern.
    bool begin(const std::string& hostname);

private:
    unsigned long lastAttemptAtMillis_ = 0;

    static constexpr unsigned long kRetryIntervalMs = 5000;
};

#endif
