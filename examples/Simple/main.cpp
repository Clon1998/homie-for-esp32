#include <WiFi.h>
#include <AsyncMqttClient.hpp>
#include <Device.hpp>

#define WIFI_SSID "yourSSID"
#define WIFI_PASSWORD "yourpass"

#define MQTT_HOST IPAddress(192, 168, 1, 10)
#define MQTT_PORT 1883

#define MQTT_USER ""
#define MQTT_PASS ""

#define DEVICE_ID "yourDeviceID"
#define DEVICE_NAME "Lean-Homie Tester"

AsyncMqttClient mqttClient;

Device homie_device(mqttClient, DEVICE_ID);
Node engine_node(homie_device, mqttClient, "engine-01");
Node switch_node(homie_device, mqttClient, "switch-01");

void createNewProperty(Node &node, const char *id, const char *name,
                       HomieDataType dataType,
                       const PropertySetCallback &callback,
                       const char *initialValue, bool settable,
                       bool retainable, const char *format,
                       const char *unit)
{

  Property *p = new Property(node, mqttClient, id, name, dataType);

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

  node.addProperty(*p);
}

void onDeviceStateChangedCallback(HomieDeviceState state)
{
  Serial.printf("Device State Changed: %d (%s) \n", state, stateEnumToString(state));
}
void onDeviceSetupDoneCallback(Device &device)
{
  Serial.println("Device setup done!");
}

void getFreeHeap(Stats &stat)
{
  stat.setValue(ESP.getFreeHeap());
}

void getUptime(Stats &stat)
{
  stat.setValue(millis() / 1000);
}

void onSwitchChanged(Property &property, const char *payload)
{
  Property *brightness = switch_node.findPropertyByID("brightness");

  if (strcasecmp("on", payload) == 0)
  {
    property.setValue("ON");
    brightness->setValue("1000000.0");
  }
  else if (strcasecmp("off", payload) == 0)
  {
    property.setValue("OFF");
    brightness->setValue("2.0");
  }
  else
  {
    log_e("payload \\%s\\ is not valid => value is not changed", payload);
  }
}

void setupHomieStuff()
{
  homie_device.setName(DEVICE_NAME);
  homie_device.onDeviceStateChanged(onDeviceStateChangedCallback);
  homie_device.onDeviceSetupDoneCallback(onDeviceSetupDoneCallback);

  homie_device.addStats("freeHeap", getFreeHeap);
  homie_device.addStats("uptime", getUptime);
  homie_device.setStatsInterval(30);

  engine_node.setName("Car Engine");
  engine_node.setType("V8");

  createNewProperty(engine_node, "speed", "Geschwindigkeit", HOMIE_INTEGER, nullptr, "0", false, true, "0:200", "km/h");
  createNewProperty(engine_node, "temperature", "Temperatur", HOMIE_FLOAT, nullptr, "20", false, true, "", "°C");

  homie_device.addNode(engine_node);

  switch_node.setName("Power switch");
  switch_node.setType("Power");

  createNewProperty(switch_node, "switch", "Schalter", HOMIE_STRING, onSwitchChanged, nullptr, true, false, "", "on/off");
  createNewProperty(switch_node, "brightness", "Helligkeit", HOMIE_FLOAT, nullptr, nullptr, false, false, "", "Candela");

  homie_device.addNode(switch_node);
}

void onWiFiEventCallback(WiFiEvent_t event)
{
  switch (event)
  {
  case SYSTEM_EVENT_WIFI_READY:
    Serial.println("SYSTEM_EVENT_WIFI_READY");
    break;
  case SYSTEM_EVENT_STA_GOT_IP:
    Serial.println("Got WiFi IP");
    break;
  case SYSTEM_EVENT_STA_DISCONNECTED:
    //FIXME: Lost WiFi needs to re-established...
    Serial.println("lost WiFi connection");
    break;
  case SYSTEM_EVENT_STA_START:
    Serial.println("SYSTEM_EVENT_STA_START");
    break;
  case SYSTEM_EVENT_STA_STOP:
    Serial.println("SYSTEM_EVENT_STA_STOP");
    break;
  case SYSTEM_EVENT_STA_CONNECTED:
    Serial.println("SYSTEM_EVENT_STA_STOP");
    break;
  default:
    Serial.printf("WiFi event %d not handled here.\n", event);
    break;
  }
}

void setup()
{
  Serial.begin(9600); // Initialize serial communications with the PC
  while (!Serial)
    ;

  mqttClient.setKeepAlive(30);
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient.setCredentials(MQTT_USER, MQTT_PASS);
  mqttClient.setKeepAlive(30);

  setupHomieStuff();

  WiFi.onEvent(onWiFiEventCallback);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

unsigned long last_millis = 0;

void send_new_readings()
{
  static bool plus = true;
  // only every 5 seconds update value
  if (millis() - last_millis > 5000)
  {
    last_millis = millis();
    Property *speed = engine_node.findPropertyByID("speed");
    Property *temp = engine_node.findPropertyByID("temperature");
    int newSpeed = speed->getValueAsInt();
    newSpeed += (plus) ? random(10) : -random(10);
    speed->setValue(newSpeed);

    float newTemperatur = String(temp->getValue()).toFloat();
    newTemperatur += (plus) ? random(1000) * 0.01 : -random(1000) * 0.01;
    temp->setValue(String(newTemperatur));

    plus = !plus;
  }
}

void loop()
{
  if (homie_device.isSetupDone())
  {
    send_new_readings();
  }
}
