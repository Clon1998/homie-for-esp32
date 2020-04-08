#pragma once

#include <string.h>
// #include <MQTT.h>
#include <AsyncMqttClient.h>
#include <HomieDatatype.hpp>

class Node;
class Property;

typedef std::function<void(Property *property)> PropertySetCallback;

class Property
{
private:
    Node *_parent;
    const char *_topic;
    const char *_topicSet;

    const char *_name;
    const char *_id;
    HomieDataType _dataType = HOMIE_UNDEFINED;
    bool _settable = false;
    bool _retained = true;
    const char *_unit;
    const char *_format;
    PropertySetCallback _callback;

    char *_value;
    size_t _valueSize;

    AsyncMqttClient *_client;
    char *prefixedPropertyTopic(char *buff, const char *d);

public:
    Property(Node *src, AsyncMqttClient *client, const char *id, const char *name, HomieDataType dataType);
    ~Property() {}

    void setup();
    void init();

    //    Node *getParent()
    //    {
    //        return this->_parent;
    //    }
    //    void setParent(Node *parent)
    //    {
    //        this->_parent = parent;
    //    }
    //
    const char *getName()
    {
        return this->_name;
    }

    //
    const char *getId()
    {
        return this->_id;
    }

    //
    //    HomieDataType getDataType()
    //    {
    //        return this->_dataType;
    //    }

    bool isSettable()
    {
        return this->_settable;
    }

    void setSettable(bool settable)
    {
        this->_settable = settable;
    }
    //
    //    bool getRetained()
    //    {
    //        return this->_retained;
    //    }
    void setRetained(bool retained)
    {
        this->_retained = retained;
    }
    //
    //    String getUnit()
    //    {
    //        return this->_unit;
    //    }
    void setUnit(const char *unit)
    {
        this->_unit = unit;
    }
    //
    //    String getFormat()
    //    {
    //        return this->_format;
    //    }
    void setFormat(const char *format)
    {
        this->_format = format;
    }
    //

    //
    const char *getTopic()
    {
        return this->_topic;
    }
    ////    void setTopic(const char *topic)
    ////    {
    ////        this->_topic = topic;
    ////        this->_topicSet = topic + "/set";
    ////    }
    ////
    const char *getTopicSet()
    {
        return this->_topicSet;
    }

    int getValueAsInt();

    long getValueAsLong();

    bool getValueAsBool();

    const char *getValue()
    {
        return this->_value;
    }

    void setValue(const char *value, bool useSetTopic = false);

    void setDefaultValue(const char *value);

    void setCallback(PropertySetCallback callback)
    {
        this->_callback = callback;
    }

    PropertySetCallback getCallback()
    {
        return _callback;
    }
};