#ifndef CONFIG_H
#define CONFIG_H

// Helper types to keep all runtime configuration in one place.
// These structures are persisted to NVS by loadSettings()/saveSettings().
#include <Arduino.h>

// Alarm limits for sensors.  Values outside of the min/max range
// trigger an event with 5 % hysteresis (see checkThreshold()).
struct Thresholds {
    float lidarMin  = 0;     // Distance in mm
    float lidarMax  = 1500;
    float smokeMin  = 0;     // MQ‑2 sensor value
    float smokeMax  = 400;
    float eco2Min   = 400;   // ENS160 eCO2 in ppm
    float eco2Max   = 2000;
    float tvocMin   = 0;     // ENS160 TVOC in ppb
    float tvocMax   = 600;
    float presMin   = -500;  // Differential pressure in Pa
    float presMax   = 500;
};

struct Settings {
    char siteName[25] = "UNDEF";        // Station identifier used in MQTT topics
    char wifiSSID[33] = "";             // Credentials for Wi‑Fi STA mode
    char wifiPass[65] = "";
    char mqttHost[65] = "broker.hivemq.com"; // MQTT broker address
    uint16_t mqttPort = 1883;           // MQTT broker port
    char mqttUser[33] = "";             // MQTT auth
    char mqttPass[65] = "";
    uint8_t mqttQos = 0;                // Default QoS for publishes
    char uiUser[17] = "admin";          // Web UI credentials (username)
    // Default password is "admin" with SHA-256 applied
    char uiPass[65] = "8c6976e5b5410415bde908bd4dee15dfb167a9c873fc4bb8a81f6f2ab448a918";
                                         // Web UI password (SHA256 hash)
    bool debugEnable = false;           // If true, publish debug messages
    Thresholds thr;                     // Per-sensor alarm limits
    uint16_t clogMin = 400;             // Distance below which chute clogging is detected (mm)
    uint8_t clogHold = 2;               // Number of consecutive readings before clog event
};

extern Settings settings;

// Load the settings structure from NVS flash.  Must be called once on start-up
// before using any fields of ::settings.
void loadSettings();

// Persist the current contents of ::settings to NVS flash.
void saveSettings();

#endif // CONFIG_H
