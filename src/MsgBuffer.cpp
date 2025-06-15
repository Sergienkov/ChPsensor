#include "MsgBuffer.h"
#include "Config.h"
#include <PubSubClient.h>

extern PubSubClient mqtt;

void bufferInit() {
    LittleFS.begin(true);
}

void bufferStore(const String& topic, const String& payload) {
    File f = LittleFS.open("/buf", FILE_APPEND);
    if(!f) return;
    f.println(topic + "|" + payload);
    f.close();
}

void bufferFlush() {
    if(!LittleFS.exists("/buf")) return;
    File f = LittleFS.open("/buf", FILE_READ);
    while(f.available()) {
        String line = f.readStringUntil('\n');
        int sep = line.indexOf('|');
        if(sep <= 0) continue;
        String topic = line.substring(0, sep);
        String payload = line.substring(sep + 1);
        mqtt.publish(topic.c_str(), payload.c_str(), settings.mqttQos, false);
    }
    f.close();
    LittleFS.remove("/buf");
}
