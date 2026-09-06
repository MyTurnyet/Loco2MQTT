#include "application/MqttCommandRouter.h"

namespace
{
    std::optional<std::pair<std::string, std::string>> parseDeviceTypeAndAddress(const std::string& topic)
    {
        const auto firstSlash = topic.find('/');
        const auto secondSlash = firstSlash == std::string::npos ? std::string::npos : topic.find('/', firstSlash + 1);
        const auto thirdSlash = secondSlash == std::string::npos ? std::string::npos : topic.find('/', secondSlash + 1);
        if (firstSlash == std::string::npos || secondSlash == std::string::npos || thirdSlash == std::string::npos)
        {
            return std::nullopt;
        }
        return std::make_pair(topic.substr(firstSlash + 1, secondSlash - firstSlash - 1),
                               topic.substr(secondSlash + 1, thirdSlash - secondSlash - 1));
    }
}

void MqttCommandRouter::update()
{
    std::optional<IncomingMqttMessage> message = mqttPort_.receiveCommand();
    while (message.has_value())
    {
        handleMessage(*message);
        message = mqttPort_.receiveCommand();
    }
}

void MqttCommandRouter::handleMessage(const IncomingMqttMessage& message)
{
    const auto parsed = parseDeviceTypeAndAddress(message.topic());
    if (!parsed.has_value())
    {
        return;
    }
    for (auto& [decoder, encoder] : decoders_)
    {
        if (!decoder->canDecode(parsed->first))
        {
            continue;
        }
        std::optional<DomainCommand> command = decoder->decode(parsed->second, message.payload());
        if (command.has_value())
        {
            encoder->encode(*command, scheduler_);
        }
        return;
    }
}
