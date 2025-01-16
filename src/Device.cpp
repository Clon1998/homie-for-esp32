#include <WiFi.h>

#include <Device.hpp>
#include "MqttLogger.hpp"

const char *stateEnumToString(HomieDeviceState e) {
    switch (e) {
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

Device::Device(AsyncMqttClient &client, const char *id, uint8_t buffSize) : _client(client),
                                                                            _extensions(nullptr) {
    char *idBuff = new char[strlen(id) + 1];
    strcpy(idBuff, id);
    _id = idBuff;
    _name = _id;

    char *topic = new char[6 + strlen(_id) + 2];  // last + 6 for range _65536, last + 4 for /set
    strcpy(topic, "homie/");
    strcat(topic, _id);
    strcat(topic, "/");
    _topic = topic;

    char *topicLw = new char[strlen(topic) + 6 + 1];  // last + 6 for range _65536, last + 4 for /set
    strcpy(topicLw, topic);
    strcat(topicLw, "$state");
    _lwTopic = topicLw;

    const char *macFromWifi = WiFi.macAddress().c_str();
    char *macBuff = new char[strlen(macFromWifi) + 1];
    strcpy(macBuff, macFromWifi);
    _mac = macBuff;

    const char *homieVersion = HOMIE_VER;
    char *versionBuff = new char[strlen(homieVersion) + 1];
    strcpy(versionBuff, homieVersion);
    _homieVersion = versionBuff;

    log_i("Setting LW to: %s", _lwTopic);
    _client.setWill(_lwTopic, 2, true, "lost");

    _client.onConnect([this](bool sessionPresent) {
        onMqttConnectCallback(sessionPresent);
    });

    _client.onDisconnect([this](AsyncMqttClientDisconnectReason reason) {
        onMqttDisconnectCallback(reason);
    });

    _client.onMessage([this](char *topicCharPtr, char *payloadCharPtr, AsyncMqttClientMessageProperties properties,
                             size_t len, size_t index, size_t total) {
        onMessageReceivedCallback(topicCharPtr, payloadCharPtr, properties, len, index, total);
    });

    WiFi.onEvent([this](WiFiEvent_t event, WiFiEventInfo_t info) {
        onWiFiEventCallback(event);
    });

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

    _wifiReconnectTimer = xTimerCreate(
        "wifiRTimer",
        pdMS_TO_TICKS(10000),
        pdFALSE,
        (void *)args,
        [](TimerHandle_t timer) {
            TimerArguments *args = (TimerArguments *)pvTimerGetTimerID(timer);
            if (WiFi.status() != WL_CONNECTED) {
                log_i("Attempting WiFi reconnection...");
                WiFi.reconnect();
            } else {
                log_i("Stopping Timer since WiFi is Connected");
                xTimerStop(timer, 0);
            }
        });

    //TODO: Add the taskWorkOnQueue to the WatchDog!

    xTaskCreateUniversal(
        this->handleIncomingMqttTaskCode,
        "homie_incoming_Mqtt",
        8192,
        this,
        4,
        &_taskNewMqttMessages,
        CONFIG_HOMIE_INCOMING_RUNNING_CORE);

    xTaskCreateUniversal(
        this->statsTaskCode,
        "homie_stats",
        8192 * 3,
        this,
        2,
        &_taskStatsHandling,
        CONFIG_HOMIE_STATS_RUNNING_CORE);
}

Node &Device::addNode(const char *id) {
    Node &n = *new Node(*this, _client, id);
    this->_nodes.push_back(&n);

    return n;
}

Node &Device::addNode(const char *id, const char *name, const char *type) {
    Node &n = addNode(id);

    n.setName(name);
    n.setType(type);
    return n;
}

void Device::setup() {
    if (!_client.connected()) {
        log_e("Tryed to init device, but MQTT client isnt connected!");
        return;
    }

    if (!_name || !_id) {
        this->setState(DSTATE_ALERT);
        log_e("The devices Name or ID is not set. Name:'%s' ID: '%s'", _name ? _name : "UNDEFINED", _id ? _id : "UNDEFINED");
        return;
    }
    //    "homie/" + id + "/"

    log_i("Device-Setup Base-Topic '%s'", _topic);
    setState(DSTATE_INIT);

    _client.publish(prefixedTopic(_workingBuffer, "$state"), 1, true, stateEnumToString(_state));
    _client.publish(prefixedTopic(_workingBuffer, "$homie"), 1, true, _homieVersion);
    _client.publish(prefixedTopic(_workingBuffer, "$name"), 1, true, _name);
    _client.publish(prefixedTopic(_workingBuffer, "$extensions"), 1, true, _extensions);
    _client.publish(prefixedTopic(_workingBuffer, "$mac"), 1, true, _mac);
    _client.publish(prefixedTopic(_workingBuffer, "$localip"), 1, true, _ip.toString().c_str());
    _client.publish(prefixedTopic(_workingBuffer, "$stats/interval"), 1, true, String(_statsInterval).c_str());

    String statIds((char *)0);
    // We only assume that every Stat is of max length 12, in my case it is!
    statIds.reserve(_stats.size() * 13);

    for (auto const &stat : _stats) {
        statIds.concat(stat->getId());
        statIds.concat(",");
    }
    if (_stats.size() > 0)
        statIds.remove(statIds.length() - 1);
    _client.publish(prefixedTopic(_workingBuffer, "$stats"), 1, true, statIds.c_str());

    String nodeNames((char *)0);
    // We only assume thath every node name max len is 12, in my case it is!
    nodeNames.reserve(_nodes.size() * 13);
    for (auto const &node : _nodes) {
        nodeNames.concat(node->getId());
        nodeNames.concat(",");
    }
    if (_nodes.size() > 0)
        nodeNames.remove(nodeNames.length() - 1);

    _client.publish(prefixedTopic(_workingBuffer, "$nodes"), 1, true, nodeNames.c_str());

    for (auto const &node : _nodes) {
        if (!node->setup()) {
            setState(DSTATE_ALERT);
            log_e("Error while setting up node: ", node->getName() ? node->getName() : "UNDEFINED", node->getId() ? node->getId() : "UNDEFINED");
        };
    }
}

char *Device::prefixedTopic(char *buff, const char *d) {
    strcpy(buff, _topic);
    strcat(buff, d);
    // log_v("Prefixed Topic Device: %s", buff);
    return buff;
}

void Device::init() {
    if (!_client.connected()) {
        log_e("Tryed to init device, but MQTT client isnt connected!");
        return;
    }

    if (!_name || !_id) {
        this->setState(DSTATE_ALERT);
        log_e("The devices Name or ID is not set.");
        return;
    }

    log_v("Device-Init Base-Topic '%s'", _topic);
    setState(DSTATE_INIT);

    MqttLogger::init(&_client, _id);

    _client.publish(prefixedTopic(_workingBuffer, "$localip"), 1, true, _ip.toString().c_str());
    for (auto const &node : _nodes) {
        node->init();
    }
}

void Device::registerSettableProperty(Property &property) {
    log_v("Added new callback to map for property %s (%s) with topic: '%s' and topicSet: '%s'",
          property.getName(), property.getId(), property.getTopic(), property.getTopicSet());

    _topicCallbacks[property.getTopicSet()] = &property;
    vTaskDelay(pdMS_TO_TICKS(20));

    if (property.isRetained()) {
        _topicCallbacks[property.getTopic()] = &property;
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
//
void Device::onMessageReceivedCallback(char *topicCharPtr, char *payloadCharPtr, AsyncMqttClientMessageProperties properties,
                                       size_t len, size_t index, size_t total) {
    if (len == 0)
        return;

    if (index + len != total) {
        log_e("Received a multipart message, not implemented yet!");
    }

    char *payload = new char[len + 1];
    strncpy(payload, payloadCharPtr, len);
    payload[len] = '\0';

    char *topic = new char[strlen(topicCharPtr) + 1];
    strcpy(topic, topicCharPtr);

    PublishQueueElement *tmp = new PublishQueueElement;
    tmp->topic = topic;
    tmp->payload = payload;
    tmp->mqttProps = properties;

    log_v("New Message on topic: '%s' with payload: '%s' len %d total %d lenin %d lenout %d retain %d", topic, payload, len, total, strlen(payloadCharPtr), strlen(payload), properties.retain);
    xQueueSendToBack(_newMqttMessageQueue, &tmp, 20 / portTICK_PERIOD_MS);
}

void Device::restoreRetainedProperties() {
    //Wait for the msgs to come in!
    vTaskDelay(pdMS_TO_TICKS(2000));
    std::map<Property *, PublishQueueElement *> receivedValues;  //from normal property topic
    std::map<Property *, PublishQueueElement *> commandValues;   //from SET topic

    unsigned long stamp = millis();

    log_i("---------------------------------------");
    log_i("Restoring retained properties... STARTED");
    log_i("Working off received Messages, splitting them into REC_MAP and CMD_MAP");
    log_i("---------------------------------------");

    PublishQueueElement *elm;
    while (xQueueReceive(_newMqttMessageQueue, &elm, 0)) {
        std::map<const char *, Property *>::iterator it = _topicCallbacks.find(elm->topic);

        if (it != _topicCallbacks.end()) {
            Property *p = it->second;
            // If Property is not retained we should not check for some retained/default values
            if (p == nullptr || !p->isRetained()) {
                delete elm;
                continue;
            }
            // Is it normal DATA channel!
            if (strcmp(elm->topic + (strlen(elm->topic) - 4), "/set") != 0) {
                //Restored from last State
                receivedValues[p] = elm;
            } else {
                //Comes from SET Channel
                if (elm->mqttProps.retain) {
                    // Message was retained, ignoring all retained msgs in SET CHANNEL! As per Homie Spec!
                    delete elm;
                } else {
                    commandValues[p] = elm;
                }
            }
        }
    }

    log_i("---------------------------------------");
    log_i("Working off values in REC_MAP, or use value in CMD_MAP if present.");
    log_i("REC_MAP-CNT: %d", receivedValues.size());
    log_i("---------------------------------------");
    auto tmpRec = receivedValues;  //Create a copy to be used!!!
    for (std::map<Property *, PublishQueueElement *>::iterator it = tmpRec.begin(); it != tmpRec.end(); it++) {
        Property *p = it->first;
        bool comesFromCommand = false;
        if (p->isSettable()) {
            // Check if we have a command value for this property, so we can use it instead of the restored data value!
            if (commandValues.count(p) > 0) {
                elm = commandValues[p];
                commandValues.erase(p);
                comesFromCommand = true;
                log_v("COMMAND CHANNEL FOR: Property %s with CMD-Value '%s' DATA-Value '%s'", p->getName(), elm->payload, it->second->payload);
            } else {
                elm = it->second;
                log_v("DATA CHANNEL FOR: Property %s with Topic %s.", p->getName(), p->getTopic());
            }
        } else {
            elm = it->second;
        }

        //Only delete the DATA channels from the subscription and topic callbacks, since we dont need them anymore!
        _topicCallbacks.erase(it->second->topic);
        _client.unsubscribe(it->second->topic);
        if (p->isRetained()) {
            PropertySetCallback callback = p->getCallback();
            if (callback == nullptr) {
                p->setValue(elm->payload);
            } else {
                String v = callback(*p, elm->payload);
                p->setValue(v);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(40));

        if (comesFromCommand)
            delete elm;
        delete it->second;
    }

    // Think about basically ignoring/just deleting stuff in commandValues, since returendValues should be more present than Command channel!
    auto tmpCommand = commandValues;
    log_i("---------------------------------------");
    log_i("Working off left overs in CMD_MAP!");
    log_i("CMD_MAP-CNT: %d", commandValues.size());
    log_i("---------------------------------------");
    for (std::map<Property *, PublishQueueElement *>::iterator it = tmpCommand.begin(); it != tmpCommand.end(); it++) {
        Property *p = it->first;
        log_i("LeftOver: Property %s with Topic %s.", p->getName(), p->getTopic());
        elm = it->second;

        _topicCallbacks.erase(p->getTopic());
        _client.unsubscribe(p->getTopic());

        PropertySetCallback callback = p->getCallback();
        if (callback == nullptr) {
            p->setValue(elm->payload);
        } else {
            String v = callback(*p, elm->payload);
            p->setValue(v);
        }
        vTaskDelay(pdMS_TO_TICKS(40));
        delete elm;
    }

    if (!_setupDone) {
        auto tmp = _topicCallbacks;
        log_i("---------------------------------------");
        log_i("Handling default value for properties");
        log_i("---------------------------------------");
        for (auto valuePair : tmp) {
            Property *p = valuePair.second;
            const char *topic = valuePair.first;
            // Check if its a DATA channel!!
            if (strcmp(topic + (strlen(topic) - 4), "/set") != 0) {
                if (p == nullptr || !p->getName() || !p->getId()) {
                    log_v("Property for Topic had sth. empty %s", topic);
                    continue;
                }

                log_i("Didnt receive a default for %s(%s) with topic: %s onTopic: %s", p->getName(),
                      p->getId(), p->getTopic(), topic);
                _client.unsubscribe(p->getTopic());

                if (p->isRetained()) {
                    PropertySetCallback callback = p->getCallback();

                    const char *providedVal = p->getValue();

                    if (!p->validateValue(providedVal)) {
                        providedVal = defaultForDataType(p->getDataType());
                    }

                    if (callback == nullptr) {
                        p->setValue(providedVal);
                    } else {
                        String v = callback(*p, providedVal);
                        p->setValue(v);
                    }
                }
                _topicCallbacks.erase(topic);
                vTaskDelay(pdMS_TO_TICKS(40));
            }
        }
        vTaskDelay(pdMS_TO_TICKS(500));

        log_i("---------------------------------------");
        log_i("Defaults handling... DONE");
        log_i("---------------------------------------");
        _setupDone = true;
        setState(DSTATE_READY);
        for (auto callback : _onDeviceSetupDoneCallbacks)
            callback(*this);
    } else {
        vTaskDelay(pdMS_TO_TICKS(500));
        setState(DSTATE_READY);
    }
    _client.publish(prefixedTopic(_workingBuffer, "$state"), 1, true, stateEnumToString(_state));
    log_i("---------------------------------------");
    log_i("Restoring retained properties... DONE took %d ms", (millis() - stamp));
    log_i("---------------------------------------");
}

void Device::onMqttConnectCallback(bool sessionPresent) {
    log_i("MQTT Connected - Starting Device Init/Setup");
    _connectionTimeStamp = millis();
    _mqttReconnectAttempts = 0;
    xTaskCreateUniversal(
        this->startInitOrSetupTaskCode,
        "homie_init_setup",
        8192,
        this,
        2,
        nullptr,
        CONFIG_HOMIE_INCOMING_RUNNING_CORE);
}

void Device::onMqttDisconnectCallback(AsyncMqttClientDisconnectReason reason) {
    log_e("Lost MQTT connection reason: %d", reason);
    setState(DSTATE_LOST);
    vTaskSuspend(_taskStatsHandling);
    vTaskSuspend(_taskNewMqttMessages);
    if (WiFi.isConnected()) {
        log_i("Starting Timer");
        uint32_t delay = min(
            (uint32_t)(2000 * pow(2, _mqttReconnectAttempts)), 
            (uint32_t)MAX_RECONNECT_DELAY
        );
        log_i("Reconnect attempt %d in %d ms", _mqttReconnectAttempts, delay);
        xTimerChangePeriod(_mqttReconnectTimer, pdMS_TO_TICKS(delay), 0);
        xTimerStart(_mqttReconnectTimer, 0);
        _mqttReconnectAttempts++;

    }
}

void Device::onDeviceStateChanged(OnDeviceStateChangedCallback callback) {
    _onDeviceStateChangedCallbacks.push_back(callback);
}

void Device::onDeviceSetupDoneCallback(OnDeviceSetupDoneCallback callback) {
    _onDeviceSetupDoneCallbacks.push_back(callback);
}

void Device::startInitOrSetupTaskCode(void *parameter) {
    Device *crntDevice = (Device *)parameter;
    log_i("---------------------------------------");
    log_i("StartOrInit Task running on Core %d name %s", xPortGetCoreID(), pcTaskGetTaskName(nullptr));
    log_i("---------------------------------------");
    if (WiFi.status() != WL_CONNECTED || !crntDevice->_client.connected()) {
        log_i("startInitOrSetupTask, wifi or device not connected: WiFi = %s Device = %s",
              WiFi.status() == WL_CONNECTED ? "CONNECTED" : "LOST",
              crntDevice->_client.connected() ? "CONNECTED" : "LOST");
        vTaskDelete(nullptr);
    }

    // After a restart of the broker its a bad idea to publish to fast!

    vTaskDelay(pdMS_TO_TICKS(1000));

    log_i("---------------------------------------");
    log_i("Device setup/init... STARTED");
    log_i("---------------------------------------");
    unsigned long stamp = millis();
    vTaskSuspend(crntDevice->_taskNewMqttMessages);

    if (crntDevice->_setupDone) {
        crntDevice->init();
        log_i("---------------------------------------");
        log_i("Device init... DONE took %d ms", (millis() - stamp));
        log_i("---------------------------------------");
    } else {
        crntDevice->setup();
        log_i("---------------------------------------");
        log_i("Device setup... DONE took %d ms", (millis() - stamp));
        log_i("---------------------------------------");
    }
    if (crntDevice->getState() == DSTATE_ALERT) {
        log_e("FATAL ERROR CREATING HOMIE DEVICE");
        vTaskDelete(nullptr);
    }
    // log_e("Pre: FreeHeap '%d' MinFreeBlock '%d'", xPortGetFreeHeapSize(), xPortGetMinimumEverFreeHeapSize());
    crntDevice->restoreRetainedProperties();
    // log_e("Post: FreeHeap '%d'  MinFreeBlock '%d'", xPortGetFreeHeapSize(), xPortGetMinimumEverFreeHeapSize());

    vTaskResume(crntDevice->_taskNewMqttMessages);
    vTaskResume(crntDevice->_taskStatsHandling);
    vTaskDelete(nullptr);
}

void Device::statsTaskCode(void *parameter) {
    Device *crntDevice = (Device *)parameter;
    vTaskSuspend(nullptr);
    for (;;) {
#ifdef TASK_VERBOSE_LOGGING
        log_v("statsTaskCode Task running on Core %d name %s", xPortGetCoreID(), pcTaskGetTaskName(nullptr));
#endif
        if (crntDevice->_stats.size() == 0) {
            log_i("Stat task suspended, no Stats in vector");
            vTaskSuspend(nullptr);
            continue;
        }

        if (WiFi.status() != WL_CONNECTED || !crntDevice->_client.connected()) {
            log_i("statsTaskCode, wifi or device not connected: WiFi = %s Device = %s",
                  WiFi.status() == WL_CONNECTED ? "CONNECTED" : "LOST",
                  crntDevice->_client.connected() ? "CONNECTED" : "LOST");
            vTaskSuspend(nullptr);
            continue;
        }

        for (auto const &stat : crntDevice->_stats) {
            stat->publish();
        }
        vTaskDelay(pdMS_TO_TICKS(crntDevice->_statsInterval * 1000));
    }
}

void Device::handleIncomingMqttTaskCode(void *parameter) {
    vTaskSuspend(nullptr);

    Device *crntDevice = (Device *)parameter;
    for (;;) {
        PublishQueueElement *elm = nullptr;
        if (xQueueReceive(crntDevice->_newMqttMessageQueue, &elm, portMAX_DELAY)) {
            const char *tPtr = elm->topic;
            auto it = crntDevice->_topicCallbacks.find(tPtr);
            log_i("MQTT: topic: '%s' payload '%s'", tPtr, elm->payload);
            if (it != crntDevice->_topicCallbacks.end()) {
                Property *p = it->second;
                
                // Add check to prevent processing our own published values
                if (strcmp(p->getValue(), elm->payload) != 0) {
                log_v("Found a matching callback for topic '%s' property: %s(%s)", tPtr,
                      p->getName(), p->getId());

                PropertySetCallback callback = p->getCallback();
                if (callback == nullptr) {
                    p->setValue(elm->payload);
                } else {
                    String v = callback(*p, elm->payload);
                    p->setValue(v);
                }
            }
            }
            delete elm;
        }
    }
}

void Device::timerCode(TimerHandle_t timer) {
    TimerArguments *args = (TimerArguments *)pvTimerGetTimerID(timer);

    if (WiFi.status() == WL_CONNECTED) {
        log_i("Connecting to MQTT...");
        args->device->_client.connect();
    } else {
        log_i("Stopping Timer since WiFi isn't Connected");
        xTimerStop(timer, 0);
    }
}
//
void Device::onWiFiEventCallback(WiFiEvent_t event) {
    switch (event) {
        case SYSTEM_EVENT_STA_CONNECTED:
            log_i("Station Connected");
            break;

        case SYSTEM_EVENT_STA_GOT_IP: {
            log_i("Received IP Adress: %s", WiFi.localIP().toString().c_str());
            log_i("Wifi Ready!");
            _wifiReconnectAttempts = 0;
            _ip = WiFi.localIP();
            _client.connect();
            break;
        }
        case SYSTEM_EVENT_STA_DISCONNECTED:
            log_e("WiFi lost connection");
            xTimerStop(_mqttReconnectTimer, 0);
            // Add WiFi reconnection attempt
            if (WiFi.status() != WL_CONNECTED) {
                uint32_t delay = min(
                    (uint32_t)(2000 * pow(2, _wifiReconnectAttempts++)), 
                    (uint32_t)MAX_RECONNECT_DELAY
                );
                log_i("Wifi-Reconnect attempt %d in %d ms", _mqttReconnectAttempts, delay);
                xTimerChangePeriod(_wifiReconnectTimer, pdMS_TO_TICKS(delay), 0);
            }
            break;
        default:
            log_i("[WiFi-event] %d", event);
    }
}

Stats &Device::addStats(const char *id, GetStatsFunction fnc) {
    Stats &s = *new Stats(*this, _client, id);
    _stats.push_back(&s);
    s.setFunc(fnc);
    if (_stats.size() == 1 && _state == DSTATE_READY) {
        vTaskResume(_taskStatsHandling);
    }
    return s;
}