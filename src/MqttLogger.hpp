#pragma once

#include <AsyncMqttClient.h>

class MqttLogger {
public:
    static void init(AsyncMqttClient* client, const char* deviceId);
    static void log(const char* format, ...);
    static AsyncMqttClient* _client;
    static const char* _deviceId;
};

// Redefine the log macros
#undef log_i
#undef log_d
#undef log_w
#undef log_e
#undef log_v

#define log_i(format, ...) MqttLogger::log(format, ##__VA_ARGS__)
#define log_d(format, ...) MqttLogger::log(format, ##__VA_ARGS__)
#define log_w(format, ...) MqttLogger::log(format, ##__VA_ARGS__)
#define log_e(format, ...) MqttLogger::log(format, ##__VA_ARGS__)
#define log_v(format, ...) MqttLogger::log(format, ##__VA_ARGS__) 