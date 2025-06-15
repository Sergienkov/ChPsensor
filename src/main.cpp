#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ESPAsyncWebServer.h>
#include <ESP32Servo.h>
#include <Update.h>
#include "Config.h"
#include "mbedtls/sha256.h"
#include "LedFSM.h"
#include "NtpSync.h"
#include "MsgBuffer.h"
#include <ArduinoJson.h>

WiFiClient espClient;
PubSubClient mqtt(espClient);
AsyncWebServer server(80);
Servo servoX, servoY;

TaskHandle_t ledTaskHandle;

String hashPassword(const char *pwd) {
    unsigned char hash[32];
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts_ret(&ctx, 0);
    mbedtls_sha256_update_ret(&ctx, (const unsigned char*)pwd, strlen(pwd));
    mbedtls_sha256_finish_ret(&ctx, hash);
    mbedtls_sha256_free(&ctx);
    char hex[65];
    for(int i=0;i<32;i++) sprintf(hex + i*2, "%02x", hash[i]);
    hex[64] = 0;
    return String(hex);
}

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
    if(!request->authenticate(settings.uiUser, settings.uiPass)) { request->requestAuthentication(); return; }
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
    if(!request->authenticate(settings.uiUser, settings.uiPass)) { request->requestAuthentication(); return; }
    StaticJsonDocument<128> doc;
    if(deserializeJson(doc, data, len)) { request->send(400, "text/plain", "Bad JSON"); return; }
    const char* p = doc["password"];
    if(p) {
        String hashed = hashPassword(p);
        strlcpy(settings.uiPass, hashed.c_str(), sizeof(settings.uiPass));
        saveSettings();
    }
    request->send(200, "text/plain", "OK");
}

void setupWeb() {
    server.on("/api/settings", HTTP_GET, [](AsyncWebServerRequest *req){
        if(!req->authenticate(settings.uiUser, settings.uiPass)) return req->requestAuthentication();
        StaticJsonDocument<512> doc = buildSettingsJson();
        String out; serializeJson(doc, out);
        req->send(200, "application/json", out);
    });
    server.on("/api/settings", HTTP_POST, [](AsyncWebServerRequest *request){
        if(!request->authenticate(settings.uiUser, settings.uiPass)) return request->requestAuthentication();
    }, NULL, handleSettingsPost);
    server.on("/api/password", HTTP_POST, [](AsyncWebServerRequest *request){
        if(!request->authenticate(settings.uiUser, settings.uiPass)) return request->requestAuthentication();
    }, NULL, handlePasswordPost);
    server.on("/api/live", HTTP_GET, [](AsyncWebServerRequest *req){
        StaticJsonDocument<256> doc;
        doc["lidar"] = 0; doc["smoke"] = 0; doc["eco2"] = 0; doc["tvoc"] = 0;
        doc["temp"] = 0; doc["rh"] = 0; doc["x"] = 0; doc["y"] = 0;
        String out; serializeJson(doc, out);
        req->send(200, "application/json", out);
    });
    server.on("/update", HTTP_POST,
        [](AsyncWebServerRequest *request){
            if(!request->authenticate(settings.uiUser, settings.uiPass))
                return request->requestAuthentication();
            bool ok = !Update.hasError();
            AsyncWebServerResponse *resp = request->beginResponse(200, "text/plain", ok ? "OK" : "FAIL");
            resp->addHeader("Connection", "close");
            request->send(resp);
            if(ok) ESP.restart();
        },
        [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
            if(!index){
                if(!request->authenticate(settings.uiUser, settings.uiPass)) return;
                if(!Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(Serial);
            }
            if(!Update.hasError()){
                if(Update.write(data, len) != len) Update.printError(Serial);
            }
            if(final){
                if(Update.end(true)) Serial.printf("Update Success: %u bytes\n", index + len);
                else Update.printError(Serial);
            }
        });
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
    delay(10);
}
