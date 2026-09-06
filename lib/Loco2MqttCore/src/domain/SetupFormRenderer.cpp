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
}

std::string renderSetupForm(const LocoNetAdapterConfig& config)
{
    return "<html><body>"
           "<p>Loco2MQTT v" + std::string(kFirmwareVersion) + "</p>"
           "<form method=\"POST\" action=\"/\">"
           "SSID: <input name=\"ssid\" value=\"" + escapeHtmlAttribute(config.wifiSsid()) + "\"><br>"
           "Password: <input name=\"password\" type=\"password\" value=\"\"><br>"
           "<input type=\"submit\" value=\"Save\">"
           "</form></body></html>";
}
