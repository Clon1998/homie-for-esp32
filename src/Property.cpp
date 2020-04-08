#include <Property.hpp>
#include <Node.hpp>
#include <Device.hpp>

static const char *TAG = "Homie-Property";

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
        return "0";
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

boolean isNumeric(const char *s)
{
    if (s == nullptr || *s == '\0' || isspace(*s))
        return 0;
    char *p;
    strtod(s, &p);
    return *p == '\0';
}

Property::Property(Node *src, AsyncMqttClient *client, const char *id, const char *name, HomieDataType dataType) : _parent(src),
                                                                                                                   _topic(nullptr),
                                                                                                                   _name(name),
                                                                                                                   _id(id),
                                                                                                                   _dataType(dataType),
                                                                                                                   _unit(nullptr),
                                                                                                                   _format(nullptr),
                                                                                                                   _value(nullptr),
                                                                                                                   _client(client)
{

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
    case HOMIE_FLOAT:
    case HOMIE_INT:
        _valueSize = 20;
        break;
    }

    _value = new char[_valueSize];
}

char *Property::prefixedPropertyTopic(char *buff, const char *d)
{
    strcpy(buff, _topic);
    strcat(buff, d);

    ESP_LOGV(TAG, "Prefixed Topic Property: %s", buff);
    return buff;
}

void Property::setup()
{
    if (!_name || !_id || _dataType == HOMIE_UNDEFINED)
    {
        ESP_LOGE(TAG, "Property Name, Dataype or ID isnt set! Name: '%s' DataType: '%s' ID: '%s'", _name, _dataType,
                 _id);
        return;
    }

    ESP_LOGV(TAG, "Setup for property %s (%s)", _name, _id);

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
        ESP_LOGE(TAG, "Property Name, Dataype or ID isnt set! Name: '%s' DataType: '%s' ID: '%s'", _name, _dataType,
                 _id);
        return;
    }
    ESP_LOGV(TAG, "Init for property %s (%s) with base topic: '%s'", _name, _id, _topic);

    if (_settable)
    {
        _parent->getParent()->registerSettableProperty(this);
        _client->subscribe(_topic, 1);
        _client->subscribe(prefixedPropertyTopic(_parent->getParent()->getWorkingBuffer(), "/set"), 1);
    }
    else
    {
        setValue(_value);
    }
}

void Property::setValue(const char *value, bool useSetTopic)
{
    if (!value)
    {
        value = defaultForDataType(_dataType);
        ESP_LOGW(TAG, "Empty String for non String dataType using default value '%s'", this->_value);
    }

    switch (_dataType)
    {
    case HOMIE_BOOL:
        if (strcmp(_value, "true") != 0 && strcmp(_value, "false") != 0)
        {
            value = defaultForDataType(_dataType);
        }
        break;
    case HOMIE_COLOR:
        if (!_format)
        {
            ESP_LOGE(TAG, "Error, no format for color given of property: %s", _name);
        }

        float h, s, v;
        if (sscanf(value, "%a,%a,%a", &h, &s, &v) == EOF)
        {
            value = defaultForDataType(_dataType);
            break;
        }
        if (h < 0 || s < 0 || v < 0)
        {
            value = defaultForDataType(_dataType);
            break;
        }
        sprintf(_value, "%.0f,%.0f,%.0f", h, s, v);
        value = _value;

        break;
    case HOMIE_ENUM:
        if (!strstr(_format, _value))
        {
            ESP_LOGE(TAG, "Error, enum not in formats");
        }
        break;
    case HOMIE_INT:
        if (!isNumeric(value))
        {
            value = defaultForDataType(_dataType);
        }
        break;
    }

    if (strlen(value) > (_valueSize - 1))
    {
        ESP_LOGE(TAG, "value '%s' length was bigger than buffer! old:new -> '%i':'%i'", value, _valueSize, strlen(value) + 1);
        delete[] _value;
        _valueSize = strlen(value) + 1;
        _value = new char[_valueSize];
    }
    strcpy(_value, value);

    ESP_LOGV(TAG, "Property %s(%s) topic->'%s' payload->'%s:::%p' bufferValue->'%s:::%p' ", _name, _id, _topic, value, &value, _value, &_value);
    if (useSetTopic)
    {
        _client->publish(prefixedPropertyTopic(_parent->getParent()->getWorkingBuffer(), "/set"), 1, true, _value);
    }
    else
        _client->publish(_topic, 1, true, _value);
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
    strcpy(_value, value);
}