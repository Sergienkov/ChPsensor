#include "MsgBuffer.h"
#include "Config.h"
#include <PubSubClient.h>
#include <vector>

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

    // Считываем файл и собираем все строки в память
    File f = LittleFS.open("/buf", FILE_READ);
    std::vector<String> lines;
    while(f.available()) {
        lines.push_back(f.readStringUntil('\n'));
    }
    f.close();

    // Публикуем строки по очереди, останавливаемся при ошибке
    size_t i = 0;
    for(; i < lines.size(); ++i) {
        String line = lines[i];
        int sep = line.indexOf('|');
        if(sep <= 0) continue;
        String topic = line.substring(0, sep);
        String payload = line.substring(sep + 1);
        if(!mqtt.publish(topic.c_str(), payload.c_str(), settings.mqttQos, false)) {
            break;
        }
    }

    // Если все сообщения отправлены, удаляем файл
    if(i == lines.size()) {
        LittleFS.remove("/buf");
    } else {
        // Иначе переписываем обратно оставшиеся строки
        File wf = LittleFS.open("/buf", FILE_WRITE);
        for(; i < lines.size(); ++i) {
            wf.println(lines[i]);
        }
        wf.close();
    }
}
