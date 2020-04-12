#include <Node.hpp>
#include <Device.hpp>
static const char *TAG = "Homie-Node";

Node::Node(Device *src, AsyncMqttClient *client) : _parent(src),
                                                   _name(nullptr),
                                                   _id(nullptr),
                                                   _type(nullptr),
                                                   _client(client)
{
}

Property *Node::addProperty(const char *id, const char *name, HomieDataType dataType)
{
    Property *p = new Property(this, _client, id, name, dataType);
    this->_properties.push_back(p);
    return p;
}

char *Node::prefixedNodeTopic(char *buff, const char *d)
{
    strcpy(buff, _id);
    strcat(buff, "/");
    strcat(buff, d);

    //ESP_LOGV(TAG, "Prefixed Topic Node: %s", buff);
    return buff;
}

void Node::setup()
{
    if (!_name || !_type || !_id)
    {
        ESP_LOGE(TAG, "Node Name, Type or ID isnt set! Name: '%s' Type: '%s' ID: '%s'", _name, _type, _id);
        return;
    }

    std::unique_ptr<char[]> nodeTopic(new char[strlen(_id) + 13]);
    // char* nodeTopic = new char[strlen(_id) + 13];// 12 for /$properties and 1 for line end

    _client->publish(_parent->prefixedTopic(_parent->getWorkingBuffer(), prefixedNodeTopic(nodeTopic.get(), "$name")), 1, true, _name);
    _client->publish(_parent->prefixedTopic(_parent->getWorkingBuffer(), prefixedNodeTopic(nodeTopic.get(), "$type")), 1, true, _type);

    String propNames((char *)0);
    // we only assume the max prop name len is 19, in my case it is!
    propNames.reserve(_properties.size() * 20);
    for (auto const &prop : _properties)
    {
        propNames.concat(prop->getId());
        propNames.concat(",");
    }
    if (_properties.size() > 0)
        propNames.remove(propNames.length() - 1);

    ESP_LOGV(TAG, "PropNames: %s", propNames.c_str());

    _client->publish(_parent->prefixedTopic(_parent->getWorkingBuffer(), prefixedNodeTopic(nodeTopic.get(), "$properties")), 1, true, propNames.c_str());

    for (auto const &prop : _properties)
    {
        prop->setup();
    }
}

void Node::init()
{
    if (!_name || !_type || !_id)
    {
        ESP_LOGE(TAG, "Node Name, Type or ID isnt set! Name: '%s' Type: '%s' ID: '%s'", _name, _type, _id);
        return;
    }

    ESP_LOGV(TAG, "Init for node %s (%s)", _name, _id);

    for (auto const &prop : _properties)
    {
        prop->init();
    }
}