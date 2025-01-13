#pragma once

#define TAG "Home_Property"

#include <string.h>
// #include <MQTT.h>
#include <AsyncMqttClient.h>

#include <HomieDatatype.hpp>

class Node;
class Property;

// The callback function receives the received MQTT payload, if it wishes to modify the property e.g. custom format return sth. or just the payload again if nothing changed!
typedef std::function<String(Property &property, const char *payload)> PropertySetCallback;

const char *defaultForDataType(HomieDataType type);

class Property {
   private:
    Node &_parent;
    const char *_topic;
    const char *_topicSet;

    const char *_name;
    const char *_id;
    HomieDataType _dataType = HOMIE_UNDEFINED;
    bool _settable = false;
    bool _retained = true;
    const char *_unit;
    const char *_format;
    const char **_format_arr = nullptr;
    PropertySetCallback _callback;

    char *_value = nullptr;
    size_t _valueSize;

    AsyncMqttClient &_client;

    char *prefixedPropertyTopic(char *buff, const char *d);
    void setupEnumNode();

   public:
    Property(Node &src, AsyncMqttClient &client, const char *id, const char *name, HomieDataType dataType);
    ~Property() {}

    bool setup();
    void init();

    /**
     * @brief checks if the value matches the format
     * return true if the value is correct!
     * Does CHECK for null/empty value!!!
     * @param value 
     * @return true value matches the Format
     * @return false value does not match the Format
     */
    bool validateValue(const char *value);

    const char *getName() {
        return this->_name;
    }

    const char *getId() {
        return this->_id;
    }

    HomieDataType getDataType() {
        return this->_dataType;
    }

    bool isSettable() {
        return this->_settable;
    }

    void setSettable(bool settable) {
        this->_settable = settable;
    }

    bool isRetained() {
        return this->_retained;
    }

    void setRetained(bool retained) {
        this->_retained = retained;
    }

    const char *getUnit() {
        return this->_unit;
    }

    void setUnit(const char *unit) {
        if (_unit)
            delete[] _unit;

        char *_unitbuff = new char[strlen(unit) + 1];
        strcpy(_unitbuff, unit);
        _unit = _unitbuff;
    }

    const char *getFormat() {
        return this->_format;
    }

    void setFormat(const char *format) {
        if (_format)
            delete[] _format;

        char *_formatbuff = new char[strlen(format) + 1];
        strcpy(_formatbuff, format);
        _format = _formatbuff;
    }

    const char *getTopic() {
        return this->_topic;
    }

    const char *getTopicSet() {
        return this->_topicSet;
    }

    int getValueAsInt();

    long getValueAsLong();

    bool getValueAsBool();

    const char *getValue() {
        return this->_value;
    }
    /**
     * @brief Set the Value of the Property,
     * if updateToMqtt is set to TRUE it will publish to the SET channel,
     * the device will receive that MQTT publish and kicks off the callback.
     * Note that the value will be copied into the value buffer of the Property object.
     * 
     * @param value 
     * @param updateToMqtt false= Normal-Channel, true= Set-Channel e.g. homie/foo/bar/set
     */
    void setValue(const char *value, bool updateToMqtt = false);

    /**
     * @brief Set the Value of the Property,
     * if updateToMqtt is set to TRUE it will publish to the SET channel,
     * the device will receive that MQTT publish and kicks off the callback.
     * Note that the value will be copied into the value buffer of the Property object.
     * 
     * @param value 
     * @param updateToMqtt false= Normal-Channel, true= Set-Channel e.g. homie/foo/bar/set
     */
    void setValue(String value, bool updateToMqtt = false);

    void setValue(int value, bool updateToMqtt = false);

    void setValue(bool value, bool updateToMqtt = false);

    void setDefaultValue(const char *value);

    void setDefaultValue(String value);

    void setCallback(PropertySetCallback callback) {
        this->_callback = callback;
    }

    PropertySetCallback getCallback() {
        return _callback;
    }
};