#include "SetupFormRenderer.h"

#include "domain/FirmwareVersion.h"

namespace
{
    void appendEscaped(std::string& out, char c)
    {
        switch (c)
        {
            case '&': out += "&amp;"; break;
            case '"': out += "&quot;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            default: out += c;
        }
    }

    std::string escapeHtmlAttribute(const std::string& value)
    {
        std::string escaped;
        for (const char c : value)
        {
            appendEscaped(escaped, c);
        }
        return escaped;
    }

    std::string jmriPortFieldValue(const LocoNetAdapterConfig& config)
    {
        return config.jmriPort() == 0 ? "" : std::to_string(config.jmriPort());
    }
}

std::string renderSetupForm(const LocoNetAdapterConfig& config)
{
    return "<html><body>"
           "<p>Loco2MQTT v" + std::string(kFirmwareVersion) + "</p>"
           "<form method=\"POST\" action=\"/\">"
           "SSID: <input name=\"ssid\" value=\"" + escapeHtmlAttribute(config.wifiSsid()) + "\"><br>"
           "Password: <input name=\"password\" type=\"password\" value=\"\"><br>"
           "JMRI Host: <input name=\"jmri_host\" value=\""
               + escapeHtmlAttribute(config.jmriHost()) + "\"><br>"
           "JMRI Port: <input name=\"jmri_port\" type=\"number\" min=\"1\" max=\"65535\" value=\""
               + jmriPortFieldValue(config) + "\"><br>"
           "<input type=\"submit\" value=\"Save\">"
           "</form></body></html>";
}
