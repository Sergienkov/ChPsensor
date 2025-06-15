#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ESPAsyncWebServer.h>
#include <ESP32Servo.h>
#include "Config.h"
#include "LedFSM.h"
#include "NtpSync.h"
#include "MsgBuffer.h"
#include "Filter.h"
#include <ArduinoJson.h>

WiFiClient espClient;
PubSubClient mqtt(espClient);
AsyncWebServer server(80);
Servo servoX, servoY;

TaskHandle_t ledTaskHandle;

// Filter objects for sensor data (SMA window of 5 samples)
SMAFilter<float,5> mq2Filter;
SMAFilter<float,5> eco2Filter;
SMAFilter<float,5> tvocFilter;
SMAFilter<float,5> tempFilter;
SMAFilter<float,5> rhFilter;

float lastMq2 = 0;
float lastEco2 = 0;
float lastTvoc = 0;
float lastTemp = 0;
float lastRh = 0;

unsigned long lastF2 = 0;
unsigned long lastHeartbeat = 0;

void connectWiFi() {
    if(strlen(settings.wifiSSID) == 0) {
        WiFi.softAP("start", "starttrats");
        ledSetState(LedState::ERROR);
        return;
    }
    WiFi.mode(WIFI_STA);
    WiFi.begin(settings.wifiSSID, settings.wifiPass);
    unsigned long start = millis();
    while(WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
        delay(500);
    }
    if(WiFi.status() != WL_CONNECTED) {
        ledSetState(LedState::ERROR);
    }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
    // handle incoming messages
}

void connectMQTT() {
    mqtt.setServer(settings.mqttHost, settings.mqttPort);
    mqtt.setCallback(mqttCallback);
    String clientId = String("client-") + String((uint32_t)ESP.getEfuseMac(), HEX);
    if(!mqtt.connect(clientId.c_str(), settings.mqttUser, settings.mqttPass)) {
        ledSetState(LedState::ERROR);
    } else {
        ledSetState(LedState::NORMAL);
    }
}

// -------- Sensor stubs and publishing helpers --------

float readMQ2() {
    return analogRead(34);
}

float readEco2() { return random(400, 2000); }
float readTvoc() { return random(0, 600); }
float readTemp() { return random(200, 300) / 10.0f; }
float readRh() { return random(300, 700) / 10.0f; }

void publishEvent(const char* name, float value) {
    char topic[64];
    snprintf(topic, sizeof(topic), "site/%s/event/%s", settings.siteName, name);
    char payload[32];
    snprintf(payload, sizeof(payload), "%.2f", value);
    if(mqtt.connected()) mqtt.publish(topic, payload, settings.mqttQos, false);
    else bufferStore(topic, payload);
}

void publishHeartbeat() {
    char topic[64];
    snprintf(topic, sizeof(topic), "site/%s/heartbeat", settings.siteName);
    StaticJsonDocument<256> doc;
    doc["smoke"] = lastMq2;
    doc["eco2"] = lastEco2;
    doc["tvoc"] = lastTvoc;
    doc["temp"] = lastTemp;
    doc["rh"] = lastRh;
    doc["heap"] = ESP.getFreeHeap();
    String out; serializeJson(doc, out);
    if(mqtt.connected()) mqtt.publish(topic, out.c_str(), settings.mqttQos, false);
    else bufferStore(topic, out);
}

void checkSensors() {
    mq2Filter.add(readMQ2());
    eco2Filter.add(readEco2());
    tvocFilter.add(readTvoc());
    tempFilter.add(readTemp());
    rhFilter.add(readRh());

    lastMq2 = mq2Filter.average();
    lastEco2 = eco2Filter.average();
    lastTvoc = tvocFilter.average();
    lastTemp = tempFilter.average();
    lastRh = rhFilter.average();

    if(lastMq2 < settings.thr.smokeMin || lastMq2 > settings.thr.smokeMax)
        publishEvent("smoke", lastMq2);
    if(lastEco2 < settings.thr.eco2Min || lastEco2 > settings.thr.eco2Max)
        publishEvent("eco2", lastEco2);
    if(lastTvoc < settings.thr.tvocMin || lastTvoc > settings.thr.tvocMax)
        publishEvent("tvoc", lastTvoc);
}

StaticJsonDocument<512> buildSettingsJson() {
    StaticJsonDocument<512> doc;
    doc["siteName"] = settings.siteName;
    auto wifi = doc.createNestedObject("wifi");
    wifi["ssid"] = settings.wifiSSID;
    wifi["password"] = settings.wifiPass;
    auto mqttj = doc.createNestedObject("mqtt");
    mqttj["host"] = settings.mqttHost;
    mqttj["port"] = settings.mqttPort;
    mqttj["user"] = settings.mqttUser;
    mqttj["pass"] = settings.mqttPass;
    mqttj["qos"]  = settings.mqttQos;
    auto thr = doc.createNestedObject("thresholds");
    auto l = thr.createNestedObject("lidar");
    l["min"] = settings.thr.lidarMin; l["max"] = settings.thr.lidarMax;
    auto s = thr.createNestedObject("smoke");
    s["min"] = settings.thr.smokeMin; s["max"] = settings.thr.smokeMax;
    auto e = thr.createNestedObject("eco2");
    e["min"] = settings.thr.eco2Min; e["max"] = settings.thr.eco2Max;
    auto t = thr.createNestedObject("tvoc");
    t["min"] = settings.thr.tvocMin; t["max"] = settings.thr.tvocMax;
    auto clog = doc.createNestedObject("clog");
    clog["clogMin"] = settings.clogMin;
    clog["clogHold"] = settings.clogHold;
    doc["debugEnable"] = settings.debugEnable;
    doc["uiUser"] = settings.uiUser;
    return doc;
}

void handleSettingsPost(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t, size_t) {
    StaticJsonDocument<512> doc;
    if(deserializeJson(doc, data, len)) {
        request->send(400, "text/plain", "Bad JSON");
        return;
    }
    strlcpy(settings.siteName, doc["siteName"] | settings.siteName, sizeof(settings.siteName));
    JsonObject wifi = doc["wifi"]; if(!wifi.isNull()) {
        strlcpy(settings.wifiSSID, wifi["ssid"] | settings.wifiSSID, sizeof(settings.wifiSSID));
        strlcpy(settings.wifiPass, wifi["password"] | settings.wifiPass, sizeof(settings.wifiPass));
    }
    JsonObject mqttj = doc["mqtt"]; if(!mqttj.isNull()) {
        strlcpy(settings.mqttHost, mqttj["host"] | settings.mqttHost, sizeof(settings.mqttHost));
        settings.mqttPort = mqttj["port"] | settings.mqttPort;
        strlcpy(settings.mqttUser, mqttj["user"] | settings.mqttUser, sizeof(settings.mqttUser));
        strlcpy(settings.mqttPass, mqttj["pass"] | settings.mqttPass, sizeof(settings.mqttPass));
        settings.mqttQos = mqttj["qos"] | settings.mqttQos;
    }
    JsonObject thr = doc["thresholds"]; if(!thr.isNull()) {
        JsonObject l = thr["lidar"]; if(!l.isNull()) { settings.thr.lidarMin = l["min"] | settings.thr.lidarMin; settings.thr.lidarMax = l["max"] | settings.thr.lidarMax; }
        JsonObject s = thr["smoke"]; if(!s.isNull()) { settings.thr.smokeMin = s["min"] | settings.thr.smokeMin; settings.thr.smokeMax = s["max"] | settings.thr.smokeMax; }
        JsonObject e = thr["eco2"]; if(!e.isNull()) { settings.thr.eco2Min = e["min"] | settings.thr.eco2Min; settings.thr.eco2Max = e["max"] | settings.thr.eco2Max; }
        JsonObject t = thr["tvoc"]; if(!t.isNull()) { settings.thr.tvocMin = t["min"] | settings.thr.tvocMin; settings.thr.tvocMax = t["max"] | settings.thr.tvocMax; }
    }
    JsonObject clog = doc["clog"]; if(!clog.isNull()) { settings.clogMin = clog["clogMin"] | settings.clogMin; settings.clogHold = clog["clogHold"] | settings.clogHold; }
    settings.debugEnable = doc["debugEnable"] | settings.debugEnable;
    const char *user = doc["uiUser"] | settings.uiUser; strlcpy(settings.uiUser, user, sizeof(settings.uiUser));
    saveSettings();
    request->send(200, "text/plain", "OK");
}

void handlePasswordPost(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t, size_t) {
    StaticJsonDocument<128> doc;
    if(deserializeJson(doc, data, len)) { request->send(400, "text/plain", "Bad JSON"); return; }
    const char* p = doc["password"];
    if(p) {
        strlcpy(settings.uiPass, p, sizeof(settings.uiPass));
        saveSettings();
    }
    request->send(200, "text/plain", "OK");
}

void setupWeb() {
    server.on("/api/settings", HTTP_GET, [](AsyncWebServerRequest *req){
        StaticJsonDocument<512> doc = buildSettingsJson();
        String out; serializeJson(doc, out);
        req->send(200, "application/json", out);
    });
    server.on("/api/settings", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL, handleSettingsPost);
    server.on("/api/password", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL, handlePasswordPost);
    server.on("/api/live", HTTP_GET, [](AsyncWebServerRequest *req){
        StaticJsonDocument<256> doc;
        doc["lidar"] = 0; // lidar not implemented
        doc["smoke"] = lastMq2;
        doc["eco2"] = lastEco2;
        doc["tvoc"] = lastTvoc;
        doc["temp"] = lastTemp;
        doc["rh"] = lastRh;
        doc["x"] = 0; doc["y"] = 0;
        String out; serializeJson(doc, out);
        req->send(200, "application/json", out);
    });
    server.on("/update", HTTP_POST, [](AsyncWebServerRequest *req){ req->send(200, "text/plain", "OK"); ESP.restart(); });
    server.begin();
}

void setup() {
    Serial.begin(115200);
    loadSettings();
    ledInit(2);
    xTaskCreate(ledTask, "led", 1024, nullptr, 1, &ledTaskHandle);
    connectWiFi();
    ntpBegin();
    connectMQTT();
    bufferInit();
    setupWeb();
    servoX.attach(4);
    servoY.attach(5);
}

void loop() {
    mqtt.loop();
    if(mqtt.connected()) bufferFlush();

    unsigned long now = millis();
    if(now - lastF2 >= 60000) {
        checkSensors();
        lastF2 = now;
    }
    if(now - lastHeartbeat >= 3600000) {
        publishHeartbeat();
        lastHeartbeat = now;
    }
    delay(10);
}
