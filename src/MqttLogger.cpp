#include "MqttLogger.hpp"
#include <stdarg.h>

AsyncMqttClient* MqttLogger::_client = nullptr;
const char* MqttLogger::_deviceId = nullptr;

void MqttLogger::init(AsyncMqttClient* client, const char* deviceId) {
    _client = client;
    _deviceId = deviceId;
}

void MqttLogger::log(const char* format, ...) {
    if (!_client || !_deviceId) return;

    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    char topic[64];
    snprintf(topic, sizeof(topic), "debug/%s", _deviceId);
    
    _client->publish(topic, 0, false, buffer);
} 