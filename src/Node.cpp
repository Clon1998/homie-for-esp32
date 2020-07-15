#include <Node.hpp>
#include <Device.hpp>

Node::Node(Device &src, AsyncMqttClient &client, const char *id) : _parent(src),
                                                                   _name(nullptr),
                                                                   _type(nullptr),
                                                                   _client(client)
{
    char *idBuff = new char[strlen(id) + 1];
    strcpy(idBuff, id);
    _id = idBuff;
}

Property &Node::addProperty(const char *id, const char *name, HomieDataType dataType)
{
    return addProperty(*new Property(*this, _client, id, name, dataType));
}

Property &Node::addProperty(Property &property)
{
    this->_properties.push_back(&property);
    return property;
}

char *Node::prefixedNodeTopic(char *buff, const char *d)
{
    strcpy(buff, _id);
    strcat(buff, "/");
    strcat(buff, d);

    //log_v("Prefixed Topic Node: %s", buff);
    return buff;
}

bool Node::setup()
{

    if (!_name || !_type || !_id)
    {
        log_e("Node Name, Type or ID isnt set! Name: '%s' Type: '%s' ID: '%s'", _name ? _name : "UNDEFINED", _type ? _type : "UNDEFINED", _id ? _id : "UNDEFINED");
        return false;
    }

    log_v("Starting Setup for Node %s (%s)", _name, _id);

    std::unique_ptr<char[]> nodeTopic(new char[strlen(_id) + 13]);
    // char* nodeTopic = new char[strlen(_id) + 13];// 12 for /$properties and 1 for line end

    _client.publish(_parent.prefixedTopic(_parent.getWorkingBuffer(), prefixedNodeTopic(nodeTopic.get(), "$name")), 1, true, _name);
    _client.publish(_parent.prefixedTopic(_parent.getWorkingBuffer(), prefixedNodeTopic(nodeTopic.get(), "$type")), 1, true, _type);

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

    log_v("PropNames: %s", propNames.c_str());

    _client.publish(_parent.prefixedTopic(_parent.getWorkingBuffer(), prefixedNodeTopic(nodeTopic.get(), "$properties")), 1, true, propNames.c_str());

    for (auto const &prop : _properties)
    {
        if (!prop->setup())
        {
            log_e("Error while setting up property: ", prop->getName() ? prop->getName() : "UNDEFINED", prop->getId() ? prop->getId() : "UNDEFINED");
            return false;
        }
    }
    return true;
}

void Node::init()
{
    log_v("Init for node %s (%s)", _name, _id);

    for (auto const &prop : _properties)
    {
        prop->init();
    }
}