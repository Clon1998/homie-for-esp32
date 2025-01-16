#pragma once

#include <AsyncMqttClient.h>
#include <esp_log.h>  // For normal ESP logging

class MqttLogger {
public:
    static void init(AsyncMqttClient* client, const char* deviceId);
    static void log(const char* tag, const char* format, ...);
    static AsyncMqttClient* _client;
    static const char* _deviceId;
};

// Store original ESP logging macros
#define ESP_LOG_I(tag, format, ...) esp_log_write(ESP_LOG_INFO, tag, format, ##__VA_ARGS__)

// Redefine logging macros to do both
#undef log_i
#undef log_d
#undef log_w
#undef log_e
#undef log_v

#define log_i(format, ...) \
    do { \
        ESP_LOG_I(TAG, format, ##__VA_ARGS__); \
        MqttLogger::log(TAG, format, ##__VA_ARGS__); \
    } while(0)

#define log_d(format, ...) \
    do { \
        ESP_LOG_D(TAG, format, ##__VA_ARGS__); \
        MqttLogger::log(TAG, format, ##__VA_ARGS__); \
    } while(0)
#define log_w(format, ...) \
    do { \
        ESP_LOG_W(TAG, format, ##__VA_ARGS__); \
        MqttLogger::log(TAG, format, ##__VA_ARGS__); \
    } while(0)
#define log_e(format, ...) \
    do { \
        ESP_LOG_E(TAG, format, ##__VA_ARGS__); \
        MqttLogger::log(TAG, format, ##__VA_ARGS__); \
    } while(0)
#define log_v(format, ...) \
    do { \
        ESP_LOG_V(TAG, format, ##__VA_ARGS__); \
        MqttLogger::log(TAG, format, ##__VA_ARGS__); \
    } while(0)
