#include "MsgBuffer.h"
#include <PubSubClient.h>

void bufferInit() {
    LittleFS.begin(true);
}

void bufferStore(const String& topic, const String& payload) {
    File f = LittleFS.open("/buf", FILE_APPEND);
    if(!f) return;
    f.println(topic + "|" + payload);
    f.close();
}

void bufferFlush(PubSubClient &client) {
    if(!LittleFS.exists("/buf")) return;
    File in = LittleFS.open("/buf", FILE_READ);
    if(!in) return;
    String remaining;
    while(in.available()) {
        String line = in.readStringUntil('\n');
        int p = line.indexOf('|');
        if(p < 0) continue;
        String topic = line.substring(0, p);
        String payload = line.substring(p+1);
        if(!client.publish(topic.c_str(), payload.c_str())) {
            remaining += line + "\n";
        }
    }
    in.close();
    File out = LittleFS.open("/buf", FILE_WRITE);
    out.print(remaining);
    out.close();
}
