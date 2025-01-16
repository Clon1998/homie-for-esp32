#include "MqttLogger.hpp"
#include <stdarg.h>

AsyncMqttClient* MqttLogger::_client = nullptr;
const char* MqttLogger::_deviceId = nullptr;

void MqttLogger::init(AsyncMqttClient* client, const char* deviceId) {
    _client = client;
    _deviceId = deviceId;
}

void MqttLogger::log(const char* tag, const char* format, ...) {
    if (!_client || !_deviceId) return;

    ESP_LOG_I(tag, "Logging to MQTT");

    char buffer[256];
    va_list args;
    va_start(args, format);
    
    // Add tag to the message
    int prefixLen = snprintf(buffer, sizeof(buffer), "[%s] ", tag);
    vsnprintf(buffer + prefixLen, sizeof(buffer) - prefixLen, format, args);
    
    va_end(args);

    char topic[64];
    snprintf(topic, sizeof(topic), "debug/%s", _deviceId);
    
    ESP_LOG_I(TAG, "Publishing to topic: %s", topic);

    _client->publish(topic, 0, false, buffer);
} 