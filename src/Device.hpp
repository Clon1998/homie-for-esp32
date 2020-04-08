#pragma once

#include <map>
#include <vector>
#include <IPAddress.h>
#include <WString.h>
#include <AsyncMqttClient.h>
#include <WiFi.h>
#include <HomieState.hpp>
#include <Node.hpp>
#include <Stats.hpp>

#define HOMIE_VER "3.0.1"
#define QUEUE_SIZE 50

typedef std::function<void(HomieDeviceState state)> OnDeviceStateChangedCallback;

struct CmpStr
{
   bool operator()(char const *a, char const *b) const
   {
       ESP_LOGV(TAG, "Comparing: a:'%s' b:'%s' result: %d",a,b,strcmp(a, b));
      return strcmp(a, b) < 0;
   }
};


struct PublishQueueElement
{
    const char* topic;
    const char* payload;

    ~PublishQueueElement() {
        ESP_LOGV(TAG, "Destructor of PupQueElm");
        delete[] topic;
        delete[] payload;
    }
};

struct TimerArguments
{
    Device *device;
};

class Device
{
private:
    HomieDeviceState _state = DSTATE_LOST;
    std::vector<Node *> _nodes;
    std::vector<Stats *> _stats;
    
    AsyncMqttClient *_client;
    IPAddress _ip;

    const char* _topic;
    const char* _lwTopic;
    const char* _homieVersion = HOMIE_VER;
    const char* _id;
    const char* _name;
    const char* _mac;
    const char* _extensions;

    bool _setupDone = false;
    bool _defaultsPublished = false;
    int _statsInterval = 60;

    char* _workingBuffer;
    unsigned long _connectionTimeStamp;

    std::map<const char*, Property *, CmpStr> _topicCallbacks;
    std::vector<OnDeviceStateChangedCallback> _onDeviceStateChangedCallbacks;

    QueueHandle_t _newMqttMessageQueue;
    
    TaskHandle_t _taskStatsHandling;
    TaskHandle_t _taskNewMqttMessages;

    TimerHandle_t _mqttReconnectTimer;

    void handleRestoredValues();

    static void startInitOrSetupTaskCode(void *parameter);

    static void handleIncomingMqttTaskCode(void *parameter);

    static void statsTaskCode(void *parameter);

    static void timerCode(TimerHandle_t timer);

    void onMqttConnectCallback(bool sessionPresent);

    void onMqttDisconnectCallback(AsyncMqttClientDisconnectReason reason);

    void onMessageReceivedCallback(char *topicCharPtr, char *payloadCharPtr, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total);

    void onWiFiEventCallback(WiFiEvent_t event);

public:
    Device(AsyncMqttClient *client, const char* id, uint8_t buffSize = 128);

    ~Device() {}
    /**
     * @brief Allocates and adds a new Node to the device
     * 
     * @param id will be assigned to the node
     * @return Node* a ptr to the created Node
     */
    Node *addNode(const char *id);

    /**
     * @brief Allocates and adds new Node to the Device with more Node Parameters
     * 
     * @param id will be assigned to the node
     * @param name will be the name of the new Node
     * @param type will be the Type of the new Node
     * @return Node* a ptr to the creeated Node
     */
    Node *addNode(const char *id, const char *name, const char *type);

    /**
     * @brief Allocates and adds a new Stats Object to the device.
     * 
     * @param id will be the id of the Stat
     * @param fnc a function that supplies the Stats Object with values to publish.
     * @return Stats* a ptr to the created Stats Object
     */
    Stats *addStats(const char *id, GetStatsFunction fnc);

    /**
     * @brief Sets up the Device
     * The Setup will only be called once. 
     * Physical Device Starts/Reboots -> MQTT Connection established -> Setup()
     */
    void setup();

    /**
     * @brief Inits the Device
     * Will be called after the setup or after a reconnection to the MQTT Server.
     */
    void init();

    /**
     * @brief Registers a settable Property.
     * Basically makes sure thath incomming MQTT Msgs can be mapped to the correct Propety.
     * @param property 
     */
    void registerSettableProperty(Property *property);

    /**
     * @brief Adds a new OnDeviceStatChangedCallback to the Callback.
     * 
     * @param callback 
     */
    void onDeviceStateChanged(OnDeviceStateChangedCallback callback);

    /**
     * @brief extends a "topic" with the baseTopic of this Device
     * Example: 
     * BaseTopic = "homie/lamp-v2-dev/"
     * d = "$stats/interval"
     * returns -> "homie/lamp-v2-dev/$stats/interval"
     * 
     * @param buff The char* buffer to work on
     * @param d * the extension String.
     * @return char* 
     */
    char* prefixedTopic(char* buff, const char* d);

    /**
     * @brief Get the State object
     * 
     * @return HomieDeviceState 
     */
    HomieDeviceState getState()
    {
        return this->_state;
    }

    /**
     * @brief Set the State object
     * 
     * @param state 
     */
    void setState(HomieDeviceState state)
    {
        this->_state = state;
        for (auto callback : _onDeviceStateChangedCallbacks)
            callback(state);
    }

    /**
     * @brief Set the Device Name
     * 
     * @param name 
     */
    void setName(const char *name)
    {
        this->_name = name;
    }

    /**
     * @brief Get the Topic object
     * 
     * @return const char* 
     */
    const char* getTopic()
    {
        return this->_topic;
    }

    /**
     * @brief Get the Working Buffer object
     * 
     * @return char* 
     */
    char* getWorkingBuffer()
    {
        return this->_workingBuffer;
    }

    /**
     * @brief Get the Connection Time Stamp object
     * 
     * @return unsigned long 
     */
    unsigned long getConnectionTimeStamp()
    {
        return this->_connectionTimeStamp;
    }

    /**
     * @brief IsSetupDone
     * 
     * @return true if setup is done
     * @return false if setup isnt done
     */
   bool isSetupDone()
   {
       return this->_setupDone;
   }

    /**
     * @brief are the defaults Published
     * 
     * @return true if the defaults are published
     * @return false if the defaults are not published
     */
   bool areDefaultsPublished()
   {
       return this->_defaultsPublished;
   }
};