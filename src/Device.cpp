#include <Device.hpp>
#include <WiFi.h>

static const char *TAG = "Homie-Device";

const char *stateEnumToString(HomieDeviceState e)
{
    switch (e)
    {
    case DSTATE_READY:
        return "ready";
    case DSTATE_SLEEPING:
        return "sleeping";
    case DSTATE_DISCONNECTED:
        return "disconnected";
    case DSTATE_ALERT:
        return "alert";
    case DSTATE_INIT:
        return "init";
    case DSTATE_LOST:
        return "lost";

    default:
        return "alert";
    }
}

Device::Device(AsyncMqttClient *client, const char *id, uint8_t buffSize) : _client(client),
                                                                            _id(id),
                                                                            _name(nullptr),
                                                                            _extensions(nullptr)
{

    char *topic = new char[6 + strlen(_id) + 2]; // last + 6 for range _65536, last + 4 for /set
    strcpy(topic, "homie/");
    strcat(topic, _id);
    strcat(topic, "/");
    _topic = topic;

    char *topicLw = new char[strlen(topic) + 6 + 1]; // last + 6 for range _65536, last + 4 for /set
    strcpy(topicLw, topic);
    strcat(topicLw, "$state");
    _lwTopic = topicLw;

    ESP_LOGI(TAG, "Setting LW to: %s", _lwTopic);
    client->setWill(_lwTopic, 1, true, "lost");

    client->onConnect(bind(&Device::onMqttConnectCallback, this, std::placeholders::_1));
    client->onDisconnect(bind(&Device::onMqttDisconnectCallback, this, std::placeholders::_1));
    client->onMessage(bind(&Device::onMessageReceivedCallback, this, std::placeholders::_1, std::placeholders::_2,
                           std::placeholders::_3, std::placeholders::_4, std::placeholders::_5, std::placeholders::_6));
    WiFi.onEvent(bind(&Device::onWiFiEventCallback, this, std::placeholders::_1));

    _workingBuffer = new char[buffSize];

    _newMqttMessageQueue = xQueueCreate(HOMIE_INCOMING_MSG_QUEUE, sizeof(PublishQueueElement *));

    TimerArguments *args = new TimerArguments();
    args->device = this;

    _mqttReconnectTimer = xTimerCreate(
        "mqttTimer",
        pdMS_TO_TICKS(10000),
        pdFALSE,
        (void *)args,
        this->timerCode);

    //TODO: Add the taskWorkOnQueue to the WatchDog!

    xTaskCreateUniversal(
        this->handleIncomingMqttTaskCode,
        "homie_incoming_Mqtt",
        8192,
        this,
        4,
        &_taskNewMqttMessages,
        -1);

    xTaskCreateUniversal(
        this->statsTaskCode,
        "homie_stats",
        8192 * 2,
        this,
        2,
        &_taskStatsHandling,
        -1);
}

Node *Device::addNode(const char *id)
{
    Node *n = new Node(this, _client);
    this->_nodes.push_back(n);
    n->setId(id);

    return n;
}

Node *Device::addNode(const char *id, const char *name, const char *type)
{
    Node *n = addNode(id);

    n->setName(name);
    n->setType(type);
    return n;
}

void Device::setup()
{
    if (!_client->connected())
    {
        ESP_LOGE(TAG, "Tryed to init device, but MQTT client isnt connected!");
        return;
    }

    if (!_name || !_id)
    {
        this->setState(DSTATE_ALERT);
        ESP_LOGE(TAG, "The devices Name or ID is not set.");
        return;
    }
    //    "homie/" + id + "/"

    ESP_LOGI(TAG, "Device-Setup Base-Topic '%s'", _topic);
    setState(DSTATE_INIT);

    _client->publish(prefixedTopic(_workingBuffer, "$state"), 1, true, stateEnumToString(_state));
    _client->publish(prefixedTopic(_workingBuffer, "$homie"), 1, true, _homieVersion);
    _client->publish(prefixedTopic(_workingBuffer, "$name"), 1, true, _name);
    _client->publish(prefixedTopic(_workingBuffer, "$extensions"), 1, true, _extensions);
    _client->publish(prefixedTopic(_workingBuffer, "$mac"), 1, true, WiFi.macAddress().c_str());
    _client->publish(prefixedTopic(_workingBuffer, "$localip"), 1, true, WiFi.localIP().toString().c_str());
    _client->publish(prefixedTopic(_workingBuffer, "$stats/interval"), 1, true, String(_statsInterval).c_str());

    String statIds((char *)0);
    // We only assume that every Stat is of max length 12, in my case it is!
    statIds.reserve(_stats.size() * 13);

    for (auto const &stat : _stats)
    {
        statIds.concat(stat->getId());
        statIds.concat(",");
    }
    if (_stats.size() > 0)
        statIds.remove(statIds.length() - 1);
    _client->publish(prefixedTopic(_workingBuffer, "$stats"), 1, true, statIds.c_str());

    String nodeNames((char *)0);
    // We only assume thath every node name max len is 12, in my case it is!
    nodeNames.reserve(_nodes.size() * 13);
    for (auto const &node : _nodes)
    {
        nodeNames.concat(node->getId());
        nodeNames.concat(",");
    }
    if (_nodes.size() > 0)
        nodeNames.remove(nodeNames.length() - 1);

    _client->publish(prefixedTopic(_workingBuffer, "$nodes"), 1, true, nodeNames.c_str());

    for (auto const &node : _nodes)
    {
        node->setup();
    }
}

char *Device::prefixedTopic(char *buff, const char *d)
{
    strcpy(buff, _topic);
    strcat(buff, d);
    // ESP_LOGV(TAG, "Prefixed Topic Device: %s", buff);
    return buff;
}

void Device::init()
{

    if (!_client->connected())
    {
        ESP_LOGE(TAG, "Tryed to init device, but MQTT client isnt connected!");
        return;
    }

    if (!_name || !_id)
    {
        this->setState(DSTATE_ALERT);
        ESP_LOGE(TAG, "The devices Name or ID is not set.");
        return;
    }

    ESP_LOGV(TAG, "Device-Init Base-Topic '%s'", _topic);
    _defaultsPublished = false;
    setState(DSTATE_INIT);

    _client->publish(prefixedTopic(_workingBuffer, "$localip"), 1, true, WiFi.localIP().toString().c_str());
    for (auto const &node : _nodes)
    {
        node->init();
    }
}

void Device::registerSettableProperty(Property *property)
{
    ESP_LOGV(TAG, "Added new callback to map for property %s (%s) with topic: '%s' and topicSet: '%s'",
             property->getName(), property->getId(), property->getTopic(), property->getTopicSet());

    _topicCallbacks[property->getTopicSet()] = property;
    vTaskDelay(pdMS_TO_TICKS(20));
    
    if (property->isRetained()) {
        _topicCallbacks[property->getTopic()] = property;
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
//
void Device::onMessageReceivedCallback(char *topicCharPtr, char *payloadCharPtr, AsyncMqttClientMessageProperties properties,
                                       size_t len, size_t index, size_t total)
{
    if (len == 0)
        return;

    if (index + len != total)
    {
        ESP_LOGE(TAG, "Received a multipart message, not implemented yet!");
    }

    char *payload = new char[len + 1];
    strncpy(payload, payloadCharPtr, len);
    payload[len] = '\0';

    char *topic = new char[strlen(topicCharPtr) + 1];
    strcpy(topic, topicCharPtr);

    PublishQueueElement *tmp = new PublishQueueElement;
    tmp->topic = topic;
    tmp->payload = payload;

    ESP_LOGV(TAG, "New Message on topic: '%s' with payload: '%s' len %d total %d lenin %d lenout %d", topic, payload, len, total, strlen(payloadCharPtr), strlen(payload));
    xQueueSendToBack(_newMqttMessageQueue, &tmp, 20 / portTICK_PERIOD_MS);
}

void Device::handleRestoredValues()
{
    vTaskSuspend(_taskNewMqttMessages);
    //Wait for the msgs to come in!
    vTaskDelay(pdMS_TO_TICKS(2000));
    std::map<Property *, PublishQueueElement *> receivedValues;
    std::map<Property *, PublishQueueElement *> commandValues;

    ESP_LOGI(TAG, "Working off received Messages, splitting them into received and commands");
    PublishQueueElement *elm;
    while (xQueueReceive(_newMqttMessageQueue, &elm, 0))
    {
        std::map<const char *, Property *>::iterator it = _topicCallbacks.find(elm->topic);

        if (it != _topicCallbacks.end())
        {
            Property *p = it->second;
            if (p == nullptr)
            {
                delete elm;
                continue;
            }

            if (strcmp(elm->topic + (strlen(elm->topic) - 4), "/set") != 0)
            {
                //Restored from last State
                receivedValues[p] = elm;
            }
            else
            {
                //Comes from SET Channel
                commandValues[p] = elm;
            }
        }
    }

    ESP_LOGI(TAG, "Working off received values, or use command value if present.");
    auto tmpRec = receivedValues;
    for (std::map<Property *, PublishQueueElement *>::iterator it = tmpRec.begin(); it != tmpRec.end(); it++)
    {
        Property *p = it->first;
        bool comesFromCommand = false;
        if (p->isSettable())
        {
            if (commandValues.count(p) > 0)
            {
                elm = commandValues[p];
                commandValues.erase(p);
                comesFromCommand = true;
                ESP_LOGV(TAG, "COMMAND CHANNEL FOR: Property %s with CMD-Value '%s' DATA-Value '%s'", p->getName(), elm->payload, it->second->payload);
            }
            else
            {
                elm = it->second;
                ESP_LOGV(TAG, "DATA CHANNEL FOR: Property %s with Topic %s.", p->getName(), p->getTopic());
            }
        }
        else
        {
            elm = it->second;
        }

        //Only delete the DATA channels from the subscription and topic callbacks, since we dont need them anymore!
        _topicCallbacks.erase(it->second->topic);
        _client->unsubscribe(it->second->topic);
        if (p->isRetained())
        {
            p->setValue(elm->payload);

            PropertySetCallback callback = p->getCallback();
            if (callback != nullptr)
            {
                callback(p);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(40));

        if (comesFromCommand)
            delete elm;
        delete it->second;
    }

    // Think about basically ignoring/just deleting stuff in commandValues, since returendValues should be more present than Command channel!
    auto tmpCommand = commandValues;
    ESP_LOGI(TAG, "Working off left overs in command channels!");
    for (std::map<Property *, PublishQueueElement *>::iterator it = tmpCommand.begin(); it != tmpCommand.end(); it++)
    {
        Property *p = it->first;
        ESP_LOGI(TAG, "LeftOver: Property %s with Topic %s.", p->getName(), p->getTopic());
        elm = it->second;

        p->setValue(elm->payload);

        PropertySetCallback callback = p->getCallback();
        if (callback != nullptr)
        {
            callback(p);
        }
        vTaskDelay(pdMS_TO_TICKS(40));
        delete elm;
    }

    if (!_defaultsPublished)
    {
        _defaultsPublished = true;
        auto tmp = _topicCallbacks;
        ESP_LOGI(TAG, "Handling defaults for properties");
        for (auto valuePair : tmp)
        {
            Property *p = valuePair.second;
            const char *topic = valuePair.first;

            if (strcmp(topic + (strlen(topic) - 4), "/set") != 0)
            {
                if (p == nullptr || !p->getName() || !p->getId())
                {
                    ESP_LOGV(TAG, "Property for Topic had sth. empty %s", topic);
                    continue;
                }

                ESP_LOGV(TAG, "Didnt receive a default for %s(%s) with topic: %s onTopic: %s", p->getName(),
                         p->getId(), p->getTopic(), topic);
                _client->unsubscribe(p->getTopic());
                p->setValue(p->getValue(), true); // pub defaults, if we didnt receive some...
                _topicCallbacks.erase(topic);
                vTaskDelay(pdMS_TO_TICKS(40));
            }
        }
        ESP_LOGI(TAG, "Defaults handling done");
        _setupDone = true;
    }
    vTaskDelay(pdMS_TO_TICKS(500));
    setState(DSTATE_READY);
    _client->publish(prefixedTopic(_workingBuffer, "$state"), 1, true, stateEnumToString(_state));

    vTaskResume(_taskNewMqttMessages);
    vTaskResume(_taskStatsHandling);
}

void Device::onMqttConnectCallback(bool sessionPresent)
{
    ESP_LOGI(TAG, "MQTT Connected - Starting Device Init/Setup");
    _connectionTimeStamp = millis();
    xTaskCreateUniversal(
        this->startInitOrSetupTaskCode,
        "homie_init_setup",
        8192,
        this,
        2,
        nullptr,
        -1);
}

void Device::onMqttDisconnectCallback(AsyncMqttClientDisconnectReason reason)
{
    ESP_LOGE(TAG, "Lost MQTT connection reason: %d", reason);
    setState(DSTATE_LOST);
    vTaskSuspend(_taskStatsHandling);
    vTaskSuspend(_taskNewMqttMessages);

    if (WiFi.isConnected())
    {
        ESP_LOGI(TAG, "Starting Timer");
        xTimerStart(_mqttReconnectTimer, 0);
    }
}

void Device::onDeviceStateChanged(OnDeviceStateChangedCallback callback)
{
    _onDeviceStateChangedCallbacks.push_back(callback);
}

void Device::startInitOrSetupTaskCode(void *parameter)
{
    Device *crntDevice = (Device *)parameter;

    ESP_LOGI(TAG, "StartOrInit Task running on Core %d name %s", xPortGetCoreID(), pcTaskGetTaskName(nullptr));
    if (WiFi.status() != WL_CONNECTED || !crntDevice->_client->connected())
    {
        ESP_LOGI(TAG, "startInitOrSetupTask, wifi or device not connected: WiFi = %s Device = %s",
                 WiFi.status() == WL_CONNECTED ? "CONNECTED" : "LOST",
                 crntDevice->_client->connected() ? "CONNECTED" : "LOST");
        vTaskDelete(nullptr);
    }

    // After a restart of the broker its a bad idea to publish to fast!

    vTaskDelay(pdMS_TO_TICKS(1000));

    if (crntDevice->_setupDone)
    {
        crntDevice->init();
    }
    else
    {
        crntDevice->setup();
    }
    if (crntDevice->getState() == DSTATE_ALERT)
    {
        ESP_LOGE(TAG, "FATAL ERROR CREATING HOMIE DEVICE");
        vTaskDelete(nullptr);
    }
    // ESP_LOGE(TAG, "Pre: FreeHeap '%d' MinFreeBlock '%d'", xPortGetFreeHeapSize(), xPortGetMinimumEverFreeHeapSize());
    crntDevice->handleRestoredValues();
    // ESP_LOGE(TAG, "Post: FreeHeap '%d'  MinFreeBlock '%d'", xPortGetFreeHeapSize(), xPortGetMinimumEverFreeHeapSize());
    vTaskDelete(nullptr);
}

void Device::statsTaskCode(void *parameter)
{
    Device *crntDevice = (Device *)parameter;
    vTaskSuspend(nullptr);
    for (;;)
    {
        ESP_LOGV(TAG, "statsTaskCode Task running on Core %d name %s", xPortGetCoreID(), pcTaskGetTaskName(nullptr));
        if (crntDevice->_stats.size() == 0)
        {
            ESP_LOGI(TAG, "Stat task suspended, no Stats in vector");
            vTaskSuspend(nullptr);
            continue;
        }

        if (WiFi.status() != WL_CONNECTED || !crntDevice->_client->connected())
        {
            ESP_LOGI(TAG, "statsTaskCode, wifi or device not connected: WiFi = %s Device = %s",
                     WiFi.status() == WL_CONNECTED ? "CONNECTED" : "LOST",
                     crntDevice->_client->connected() ? "CONNECTED" : "LOST");
            vTaskSuspend(nullptr);
            continue;
        }

        for (auto const &stat : crntDevice->_stats)
        {
            stat->publish();
        }
        vTaskDelay(pdMS_TO_TICKS(crntDevice->_statsInterval * 1000));
    }
}

void Device::handleIncomingMqttTaskCode(void *parameter)
{
    vTaskSuspend(nullptr);

    Device *crntDevice = (Device *)parameter;
    for (;;)
    {
        PublishQueueElement *elm = nullptr;
        if (xQueueReceive(crntDevice->_newMqttMessageQueue, &elm, portMAX_DELAY))
        {
            const char *tPtr = elm->topic;
            auto it = crntDevice->_topicCallbacks.find(tPtr);
            ESP_LOGV(TAG, "MQTT: topic: '%s' payload '%s'", tPtr, elm->payload);
            if (it != crntDevice->_topicCallbacks.end())
            {
                Property *p = it->second;
                ESP_LOGV(TAG, "Found a matching callback for topic '%s' property: %s(%s)", tPtr,
                         p->getName(), p->getId());

                p->setValue(elm->payload);

                PropertySetCallback callback = p->getCallback();
                if (callback != nullptr)
                {
                    callback(p);
                }
            }
            delete elm;
        }
    }
}

void Device::timerCode(TimerHandle_t timer)
{

    TimerArguments *args = (TimerArguments *)pvTimerGetTimerID(timer);

    if (WiFi.status() == WL_CONNECTED)
    {
        ESP_LOGI(TAG, "Connecting to MQTT...");
        args->device->_client->connect();
    }
    else
    {
        ESP_LOGI(TAG, "Stopping Timer since WiFi isnt Connected");
        xTimerStop(timer, 0);
    }
}
//
void Device::onWiFiEventCallback(WiFiEvent_t event)
{
    // ESP_LOGV(TAG, "[WiFi-event] event: %d\n", event);
    switch (event)
    {
    case SYSTEM_EVENT_STA_CONNECTED:
        ESP_LOGI(TAG, "Station Connected");
        break;

    case SYSTEM_EVENT_STA_GOT_IP:
    {
        ESP_LOGI(TAG, "Received IP Adress: %s", WiFi.localIP().toString().c_str());
        ESP_LOGI(TAG, "Wifi Ready!");
        _client->connect();
        // xTimerStart(_mqttReconnectTimer, 0);
        break;
    }
    case SYSTEM_EVENT_STA_DISCONNECTED:
        ESP_LOGE(TAG, "WiFi lost connection");
        xTimerStop(_mqttReconnectTimer, 0);
        break;
    default:
        ESP_LOGI(TAG, "[WiFi-event] %d", event);
    }
}

Stats *Device::addStats(const char *id, GetStatsFunction fnc)
{
    Stats *s = new Stats(this, _client, id);
    _stats.push_back(s);
    s->setFunc(fnc);

    return s;
}