# homie-for-esp32

I created this library for the ESP32 because i wanted to utilize and play around with the xCreateTask/multitasking of the ESP32.
While this library does not implement all of the Homie Convention features e.g. Broadcasts are missing, it could be a good base to implement more features.

Currently I cant provide any additional documentation for this project besides the example and the method descriptions/code comments.

This project was developed and tested with the Homie Controller of OpenHab.

## Example
```
AsyncMqttClient mqttClient;


Device device = new Device(&mqttClient, "my-test-device");

Node *lampNode = device->addNode("lamp", "Lightcontroller", "ArtLamp Exodus");

device->addStats("freeHeap", getFreeHeap);

createNewProperty(lampNode, "brightness", HOMIE_FLOAT,
                                           brigthnessCallback, "0", true,
                                           true, "0:1", "%");

....


void brigthnessCallback(Property *property)
{
    char *pEnd;
    double perc = std::strtod(property->getValue(), &pEnd);
    //log_i("%s, %f", property->getValue(), perc);
    if (perc > 1)
    {
        perc = perc / 100;
    }
    setBrightness(255 * perc);
}

void getFreeHeap(Stats *stat)
{
    stat->setValue(ESP.getFreeHeap());
}


...

```
For the complete Example check the example folder