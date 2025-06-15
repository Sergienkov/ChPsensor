#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ESPAsyncWebServer.h>
#include <ESP32Servo.h>
#include "Config.h"
#include "LedFSM.h"
#include "NtpSync.h"
#include "MsgBuffer.h"
#include <ArduinoJson.h>

WiFiClient espClient;
PubSubClient mqtt(espClient);
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
Servo servoX, servoY;

TaskHandle_t sensorsTaskHandle;
static int servoXAngle = 90;
static int servoYAngle = 90;
static volatile uint32_t lidarDueMs = 0;

void wsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
             AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if(type != WS_EVT_DATA) return;
    String msg = String((char*)data).substring(0, len);
    if(msg == "X+") { servoXAngle = min(180, servoXAngle + 1); servoX.write(servoXAngle); }
    else if(msg == "X-") { servoXAngle = max(0, servoXAngle - 1); servoX.write(servoXAngle); }
    else if(msg == "Y+") { servoYAngle = min(180, servoYAngle + 1); servoY.write(servoYAngle); }
    else if(msg == "Y-") { servoYAngle = max(0, servoYAngle - 1); servoY.write(servoYAngle); }
    else return;
    lidarDueMs = millis() + 100;
}

TaskHandle_t ledTaskHandle;

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

float readLidar() {
    // Placeholder for real SF11c read
    return 500.0f;
}

void sensorsTask(void*) {
    const TickType_t delayStep = pdMS_TO_TICKS(100);
    const uint32_t lidarPeriod = 600000; // 10 minutes
    uint32_t lastLidar = 0;
    uint8_t clogCnt = 0;
    for(;;) {
        uint32_t now = millis();
        if((now - lastLidar >= lidarPeriod) || (lidarDueMs && now >= lidarDueMs)) {
            float dist = readLidar();
            lastLidar = now;
            lidarDueMs = 0;
            if(dist < settings.clogMin) {
                if(++clogCnt >= settings.clogHold) {
                    ledSetState(LedState::ALARM);
                }
            } else {
                clogCnt = 0;
                ledSetState(LedState::NORMAL);
            }
        }
        vTaskDelay(delayStep);
    }
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
        doc["lidar"] = 0; doc["smoke"] = 0; doc["eco2"] = 0; doc["tvoc"] = 0;
        doc["temp"] = 0; doc["rh"] = 0; doc["x"] = 0; doc["y"] = 0;
        String out; serializeJson(doc, out);
        req->send(200, "application/json", out);
    });
    server.on("/update", HTTP_POST, [](AsyncWebServerRequest *req){ req->send(200, "text/plain", "OK"); ESP.restart(); });
    ws.onEvent(wsEvent);
    server.addHandler(&ws);
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
    servoX.write(servoXAngle);
    servoY.write(servoYAngle);
    xTaskCreatePinnedToCore(sensorsTask, "sensors", 4096, nullptr, 1, &sensorsTaskHandle, 1);
}

void loop() {
    mqtt.loop();
    if(mqtt.connected()) bufferFlush();
    delay(10);
}
