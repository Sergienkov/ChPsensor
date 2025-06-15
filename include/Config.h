#ifndef CONFIG_H
#define CONFIG_H
#include <Arduino.h>

struct Settings {
    char siteName[25] = "UNDEF";
    char wifiSSID[33] = "";
    char wifiPass[65] = "";
    char mqttHost[65] = "broker.hivemq.com";
    uint16_t mqttPort = 1883;
    char mqttUser[33] = "";
    char mqttPass[65] = "";
    uint8_t mqttQos = 0;
    char uiUser[17] = "admin";
    char uiPass[33] = "admin";
    bool debugEnable = false;
};

extern Settings settings;

void loadSettings();
void saveSettings();

#endif // CONFIG_H
