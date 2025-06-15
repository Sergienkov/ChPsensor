#include "Debug.h"
#include "Config.h"
#include <PubSubClient.h>

extern PubSubClient mqtt;

void debugPublish(const String& msg) {
    if(!settings.debugEnable) return;
    char topic[64];
    snprintf(topic, sizeof(topic), "site/%s/debug", settings.siteName);
    if(mqtt.connected()) {
        mqtt.publish(topic, msg.c_str(), settings.mqttQos, false);
    }
}
