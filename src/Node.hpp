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

    const char *_name;
    const char *_id;
    const char *_type;
    std::vector<Property *> _properties;
    AsyncMqttClient *_client;

    char *prefixedNodeTopic(char *buff, const char *d);

public:
    Node(Device *src, AsyncMqttClient *client, const char *id);
    ~Node();

    void setup();

    void init();

    /**
     * @brief This method creates a new Property with the given 
     * name,id,dataType, adds it to the node and returns the property.
     * 
     * @param id 
     * @param name 
     * @param dataType 
     * @return Property* the newly created and added property
     */
    Property *addProperty(const char *id, const char *name, HomieDataType dataType);

    /**
     * @brief This method adds a Property to the node.
     * 
     * @param property the Property to add
     * @return Property* the Property added
     */
    Property *addProperty(Property *property);

    Device *getParent()
    {
        return this->_parent;
    }

    const char *getName()
    {
        return this->_name;
    }

    void setName(const char *name)
    {
        if (_name)
            delete[] _name;

        char *_namebuff = new char[strlen(name) + 1];
        strcpy(_namebuff, name);
        _name = _namebuff;
    }

    const char *getId()
    {
        return this->_id;
    }

    const char *getType()
    {
        return this->_type;
    }

    void setType(const char *type)
    {
        if (_type)
            delete[] _type;

        char *_typebuff = new char[strlen(type) + 1];
        strcpy(_typebuff, type);
        _type = _typebuff;
    }
};
