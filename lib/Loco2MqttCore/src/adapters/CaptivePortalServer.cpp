#ifdef ARDUINO

#include "CaptivePortalServer.h"

#include <WiFi.h>

CaptivePortalServer::CaptivePortalServer(WebFormCommissioningAdapter& formAdapter)
    : formAdapter_(formAdapter), webServer_(80)
{
}

void CaptivePortalServer::begin()
{
    WiFi.softAP(kApName);
    dnsServer_.start(kDnsPort, "*", WiFi.softAPIP());
    webServer_.on("/", HTTP_GET, [this]() { handleRoot(); });
    webServer_.on("/", HTTP_POST, [this]() { handleSubmit(); });
    webServer_.onNotFound([this]() { handleRoot(); });
    webServer_.begin();
}

void CaptivePortalServer::update()
{
    dnsServer_.processNextRequest();
    webServer_.handleClient();
}

void CaptivePortalServer::handleRoot()
{
    webServer_.send(200, "text/html", formAdapter_.renderPage().c_str());
}

void CaptivePortalServer::handleSubmit()
{
    const std::string ssid = webServer_.arg("ssid").c_str();
    const std::string password = webServer_.arg("password").c_str();
    if (!formAdapter_.wouldAccept(ssid, password))
    {
        webServer_.send(200, "text/html", "Missing SSID or password - not saved.");
        return;
    }
    webServer_.send(200, "text/html", "Saved. Rebooting...");
    formAdapter_.handleSubmission(ssid, password);
}

#endif
