#ifndef CONFIG_H
#define CONFIG_H
#include <Arduino.h>

struct Thresholds {
    float lidarMin  = 0;
    float lidarMax  = 1500;
    float smokeMin  = 0;
    float smokeMax  = 400;
    float eco2Min   = 400;
    float eco2Max   = 2000;
    float tvocMin   = 0;
    float tvocMax   = 600;
};

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
    Thresholds thr;
    uint16_t clogMin = 400;
    uint8_t clogHold = 2;
};

extern Settings settings;

void loadSettings();
void saveSettings();

#endif // CONFIG_H
