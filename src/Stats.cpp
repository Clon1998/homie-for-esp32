#include <Stats.hpp>
#include <Node.hpp>
#include <Device.hpp>


Stats::Stats(Device *src, AsyncMqttClient *client, const char *statName) : _parent(src),
                                                                           _name(statName),
                                                                           _id(statName),
                                                                           _value(nullptr),
                                                                           _client(client),
                                                                           _func(nullptr)
{

    char *topic = new char[strlen(_parent->getTopic()) + 7 + strlen(_id) + 1];
    stpcpy(topic, src->getTopic());
    strcat(topic, "$stats/");
    strcat(topic, _id);

    _topic = topic;
}

void Stats::publish()
{
    _func(this);
    log_v("Stats %s topic: %s value %s", _name, _topic, _value);
    _client->publish(_topic, 1, true, _value);
}