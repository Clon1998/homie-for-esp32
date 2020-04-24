#include <Property.hpp>
#include <Node.hpp>
#include <Device.hpp>

const char *dateTypeEnumToString(HomieDataType type)
{
    switch (type)
    {
    case HOMIE_STRING:
        return "string";
    case HOMIE_BOOL:
        return "boolean";
    case HOMIE_COLOR:
        return "color";
    case HOMIE_ENUM:
        return "enum";
    case HOMIE_FLOAT:
        return "float";
    case HOMIE_INT:
        return "integer";
    default:
        return "string";
    }
}

const char *defaultForDataType(HomieDataType type)
{
    switch (type)
    {
    case HOMIE_STRING:
        return "";
    case HOMIE_BOOL:
        return "false";
    case HOMIE_COLOR:
        return "0,0,0";
    case HOMIE_ENUM:
        return "default";
    case HOMIE_FLOAT:
        return "0.0";
    case HOMIE_INT:
        return "0";
    default:
        return "";
    }
}

const char *boolToString(bool b)
{
    return b ? "true" : "false";
}

bool isNumeric(const char *s)
{
    if (s == nullptr || *s == '\0' || isspace(*s))
        return false;
    char *p;
    strtod(s, &p);
    return *p == '\0';
}

Property::Property(Node *src, AsyncMqttClient *client, const char *id, const char *name, HomieDataType dataType) : _parent(src),
                                                                                                                   _topic(nullptr),
                                                                                                                   _topicSet(nullptr),
                                                                                                                   _dataType(dataType),
                                                                                                                   _unit(nullptr),
                                                                                                                   _format(nullptr),
                                                                                                                   _value(nullptr),
                                                                                                                   _client(client)
{
    char *idBuff = new char[strlen(id) + 1];
    strcpy(idBuff, id);
    _id = idBuff;

    char *nameBuff = new char[strlen(name) + 1];
    strcpy(nameBuff, id);
    _name = nameBuff;

    switch (_dataType)
    {
    case HOMIE_STRING:
        _valueSize = 201; // Limit strings to 200chars for now!
        break;

    case HOMIE_BOOL:
        _valueSize = 6;
        break;
    case HOMIE_COLOR:
        _valueSize = 12;
        break;
    case HOMIE_ENUM:
        _valueSize = 4;
        break;
    case HOMIE_UNDEFINED:
    case HOMIE_FLOAT:
    case HOMIE_INT:
        _valueSize = 20;
        break;
    }

    _value = new char[_valueSize];
    _value[0] = 0;
}

char *Property::prefixedPropertyTopic(char *buff, const char *d)
{
    strcpy(buff, _topic);
    strcat(buff, d);

    //log_v("Prefixed Topic Property: %s", buff);
    return buff;
}

void Property::setup()
{
    if (!_name || !_id || _dataType == HOMIE_UNDEFINED)
    {
        log_e("Property Name, Dataype or ID isnt set! Name: '%s' DataType: '%s' ID: '%s'", _name, _dataType,
              _id);
        return;
    }

    log_v("Setup for property %s (%s)", _name, _id);

    Device *device = _parent->getParent();

    //  homie/device/nodeid/propid/$retained
    char *topic = new char[strlen(device->getTopic()) + strlen(_parent->getId()) + 1 + strlen(_id) + 1];
    strcpy(topic, device->getTopic());
    strcat(topic, _parent->getId());
    strcat(topic, "/");
    strcat(topic, _id);

    _topic = topic;

    char *topicSet = new char[strlen(_topic) + 4 + 1];
    strcpy(topicSet, _topic);
    strcat(topicSet, "/set");
    _topicSet = topicSet;

    _client->publish(prefixedPropertyTopic(device->getWorkingBuffer(), "/$name"), 1, true, _name);
    _client->publish(prefixedPropertyTopic(device->getWorkingBuffer(), "/$datatype"), 1, true, dateTypeEnumToString(_dataType));
    _client->publish(prefixedPropertyTopic(device->getWorkingBuffer(), "/$settable"), 1, true, boolToString(_settable));
    _client->publish(prefixedPropertyTopic(device->getWorkingBuffer(), "/$retained"), 1, true, boolToString(_retained));

    if (_unit)
        _client->publish(prefixedPropertyTopic(device->getWorkingBuffer(), "/$unit"), 1, true, _unit);

    if (_format)
        _client->publish(prefixedPropertyTopic(device->getWorkingBuffer(), "/$format"), 1, true, _format);

    init();
}

void Property::init()
{
    if (!_name || !_id || _dataType == HOMIE_UNDEFINED)
    {
        log_e("Property Name, Dataype or ID isnt set! Name: '%s' DataType: '%s' ID: '%s'", _name, _dataType,
              _id);
        return;
    }
    log_v("Init for property %s (%s) with base topic: '%s'", _name, _id, _topic);

    if (_retained)
        _client->subscribe(_topic, 1);

    if (_settable)
    {
        _parent->getParent()->registerSettableProperty(this);
        _client->subscribe(prefixedPropertyTopic(_parent->getParent()->getWorkingBuffer(), "/set"), 1);
    }
}

void Property::setValue(const char *value, bool updateToMqtt)
{
    if (!value || strlen(value) == 0)
    {
        value = defaultForDataType(_dataType);
        log_w("Empty String for non String dataType using default value '%s'", this->_value);
    }
    else if (!validateValue(value))
    {
        value = defaultForDataType(_dataType);
    }

    // This is a correction incase the RGB values where send as floats -- OpenHab specific IMPL/fix
    if (_dataType == HOMIE_COLOR)
    {
        float h, s, v;
        sscanf(value, "%a,%a,%a", &h, &s, &v);
        sprintf(_value, "%.0f,%.0f,%.0f", h, s, v);
        value = _value;
    }

    if (strlen(value) > (_valueSize - 1))
    {
        log_e("value '%s' length was bigger than buffer! Allocating bigger Buffer! old:new -> '%i':'%i'", value, _valueSize, strlen(value) + 1);
        delete[] _value;
        _valueSize = strlen(value) + 1;
        _value = new char[_valueSize];
    }
    strcpy(_value, value);

    log_v("Property %s(%s) topic->'%s' payload->'%s:::%p' bufferValue->'%s:::%p' ", _name, _id, _topic, value, &value, _value, &_value);
    if (!_retained)
        return;

    if (updateToMqtt)
        _client->publish(prefixedPropertyTopic(_parent->getParent()->getWorkingBuffer(), "/set"), 1, true, _value);
    else
        _client->publish(_topic, 1, true, _value);
}

void Property::setValue(String value, bool updateToMqtt)
{
    setValue(value.c_str(), updateToMqtt);
}

void Property::setValue(int value, bool updateToMqtt)
{
    setValue(String(value).c_str(), updateToMqtt);
}

void Property::setValue(bool value, bool updateToMqtt)
{
    setValue(boolToString(value), updateToMqtt);
}

bool Property::validateValue(const char *value)
{
    switch (_dataType)
    {
    case HOMIE_BOOL:
        if (strcmp(_value, "true") != 0 && strcmp(_value, "false") != 0)
            return false;
        break;
    case HOMIE_COLOR:
        if (!_format)
        {
            log_e("Error, no format for color given of property: %s, unable to validate value. Falling back to default value", _name);
            return false;
        }

        float h, s, v;
        if (sscanf(value, "%a,%a,%a", &h, &s, &v) == EOF)
        {
            return false;
        }
        if (h < 0 || s < 0 || v < 0)
        {
            return false;
        }

        break;
    case HOMIE_ENUM:
        if (!strstr(_format, _value))
        {
            return false;
        }
        break;
    case HOMIE_INT:
        if (!isNumeric(value))
        {
            return false;
        }
        break;
    case HOMIE_FLOAT:
        log_w("Float validation not implemented yet!");
        break;

    case HOMIE_STRING:
        log_w("String validation not implemented yet!");
        break;

    default:
        return false;
    }
    return true;
}

int Property::getValueAsInt()
{
    return atoi(_value);
}

long Property::getValueAsLong()
{
    return atol(_value);
}

bool Property::getValueAsBool()
{
    return strcmp(_value, "true") == 0;
}

void Property::setDefaultValue(const char *value)
{
    if (_retained)
        strcpy(_value, value);
    else
        log_e(" A non retained Property should never have a default value!");
}

void Property::setDefaultValue(String value)
{
    setDefaultValue(value.c_str());
}