#pragma once

#include <AsyncMqttClient.h>
#include <HomieDatatype.hpp>

class Device;
class Stats;

typedef std::function<void(Stats*)> GetStatsFunction;

class Stats
{
private:
    Device *_parent;
    const char * _topic;

    const char * _name;
    const char * _id;

    const char * _value;
    AsyncMqttClient *_client;
    GetStatsFunction _func;

public:
    Stats(Device *src, AsyncMqttClient *client, const char * statName);
    ~Stats() {}

    void publish();


    const char * getId()
    {
        return _id;
    }

    void setFunc(GetStatsFunction func) {
        this->_func = func;
    }

    void setValue(const char *value)
    {
        if (_value)
            delete[] _value;
        char* valueBuff = new char[strlen(value)+1];
        strcpy(valueBuff, value);
        this->_value = valueBuff;
    }

    void setValue(String value)
    {
        setValue(value.c_str());
    }

    void setValue(int value) {
        setValue(String(value).c_str());
    }

    const char * getValue()
    {
        return _value;
    }
};