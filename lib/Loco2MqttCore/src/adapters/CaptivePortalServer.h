#pragma once

#ifdef ARDUINO

#include <cstdint>

#include <DNSServer.h>
#include <WebServer.h>

#include "adapters/WebFormCommissioningAdapter.h"

class CaptivePortalServer
{
public:
    explicit CaptivePortalServer(WebFormCommissioningAdapter& formAdapter);

    void begin();
    void update();

private:
    void handleRoot();
    void handleSubmit();

    WebFormCommissioningAdapter& formAdapter_;
    DNSServer dnsServer_;
    WebServer webServer_;

    static constexpr const char* kApName = "Loco2MQTT-Setup";
    static constexpr uint8_t kDnsPort = 53;
};

#endif
