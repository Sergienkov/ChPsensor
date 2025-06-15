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

    File rf = LittleFS.open("/buf", FILE_READ);
    if(!rf) return;

    File wf;               // Создается только при ошибке отправки
    bool failure = false;

    while(rf.available()) {
        String line = rf.readStringUntil('\n');
        int sep = line.indexOf('|');
        if(sep <= 0) continue;
        String topic   = line.substring(0, sep);
        String payload = line.substring(sep + 1);

        if(!failure && mqtt.publish(topic.c_str(), payload.c_str(), settings.mqttQos, false)) {
            continue;           // успешно отправлено, переходим к следующей строке
        }

        if(!failure) {
            // первая ошибка, открываем временный файл и записываем текущую строку
            failure = true;
            wf = LittleFS.open("/buf.tmp", FILE_WRITE);
            if(!wf) {
                // если не удалось открыть файл, выходим без изменений
                rf.close();
                return;
            }
        }
        wf.println(line);       // сохраняем текущую или последующие строки
    }

    rf.close();

    if(!failure) {
        LittleFS.remove("/buf");
    } else {
        wf.close();
        LittleFS.remove("/buf");
        LittleFS.rename("/buf.tmp", "/buf");
    }
}
