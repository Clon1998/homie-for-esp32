#include <Arduino.h>
#include <AsyncMqttClient.hpp>
#include <Device.hpp>

#define WIFI_SSID "yourSSID"
#define WIFI_PASSWORD "yourpass"

#define MQTT_HOST IPAddress(192, 168, 1, 10)
#define MQTT_PORT 1883

#define MQTT_USER ""
#define MQTT_PASS ""

#define DEVICE_ID "yourDeviceID"
#define DEVICE_NAME "yourDeviceName"

AsyncMqttClient mqttClient;
Device device(&mqttClient, DEVICE_ID);

Property *createNewProperty(Node *node, const char *id, HomieDataType dataType, const PropertySetCallback &callback,
                            const char *initialValue, bool settable, bool retainable, const char *format,
                            const char *unit)
{

  Property *p = node->addProperty(id, id, dataType);

  p->setSettable(settable);
  p->setRetained(retainable);

  if (initialValue)
    p->setDefaultValue(initialValue);
  if (unit)
    p->setUnit(unit);
  if (format)
    p->setFormat(format);
  if (callback)
    p->setCallback(callback);

  return p;
}

void onDeviceStateChangedCallback(HomieDeviceState state)
{
  log_i("Device State Changed: %d", state);
}

void onColorChanged(Property *property)
{
  log_i("HomierProperty Value HSV: %s", property->getValue());
}

void getFreeHeap(Stats *stat)
{
  stat->setValue(ESP.getFreeHeap());
}

Property *speedProp;
Property *tempProp;
void setupHomieStuff()
{
  device.setName(DEVICE_NAME);
  device.onDeviceStateChanged(onDeviceStateChangedCallback);

  device.addStats("freeHeap", getFreeHeap);

  device.setStatsInterval(30);

  Node *engineNode = device.addNode("engine", "Car Engine", "V8");
  Node *lightsNode = device.addNode("lights", "Car lighting", "Exterior lighting");

  speedProp = createNewProperty(engineNode, "speed", HOMIE_INTEGER, nullptr, "0", false, true, "0:200", "");
  tempProp = createNewProperty(engineNode, "temperature", HOMIE_FLOAT, nullptr, "0", false, true, "", "°C");

  createNewProperty(lightsNode, "color", HOMIE_COLOR, onColorChanged, "0,0,0", true, true, "hsv", "");
}

void setup()
{
  Serial.begin(115200);
  WiFi.disconnect(true);

  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient.setCredentials(MQTT_USER, MQTT_PASS);
  mqttClient.setKeepAlive(30);

  setupHomieStuff();

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void loop()
{
  int newSpeed = speedProp->getValueAsInt() + random(5);
  speedProp->setValue(newSpeed);

  vTaskDelay(pdMS_TO_TICKS(1000));
}