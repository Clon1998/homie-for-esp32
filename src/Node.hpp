#pragma once

#include <string.h>
#include <vector>
// #include <MQTT.h>
#include <AsyncMqttClient.h>
#include <Property.hpp>
class Device;

class Node
{
private:
    Device *_parent;

    const char* _name;
    const char* _id;
    const char* _type;
    std::vector<Property *> _properties;
    AsyncMqttClient *_client;

    char* prefixedNodeTopic(char* buff, const char* d);
public:
    Node(Device *src, AsyncMqttClient *client);
    ~Node();

    Property *addProperty(const char *id, const char *name, HomieDataType dataType);
    void setup();
    void init();

    Device *getParent()
    {
        return this->_parent;
    }
//    void setParent(Device *parent)
//    {
//        this->_parent = parent;
//    }
//
//    String getName()
//    {
//        return this->_name;
//    }
    void setName(const char *name)
    {
        this->_name = name;
    }

    const char* getId()
    {
        return this->_id;
    }
    void setId(const char *id)
    {
        this->_id = id;
    }
//
//    String getType()
//    {
//        return this->_type;
//    }
    void setType(const char *type)
    {
        this->_type = type;
    }
//
//    std::vector<Property *> getProperties()
//    {
//        return this->_properties;
//    }
//    void setProperties(std::vector<Property *> properties)
//    {
//        this->_properties = properties;
//    }
};
