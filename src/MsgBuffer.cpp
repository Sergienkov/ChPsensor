#include "MsgBuffer.h"

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
    // In real implementation, publish to MQTT
    while(f.available()) {
        String line = f.readStringUntil('\n');
        // publish logic here
    }
    f.close();
    LittleFS.remove("/buf");
}
