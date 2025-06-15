#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ESPAsyncWebServer.h>
#include <ESP32Servo.h>
#include <algorithm>
#include "Config.h"
#include "mbedtls/sha256.h"
#include "LedFSM.h"
#include "NtpSync.h"
#include "MsgBuffer.h"
#include "Filter.h"
#include <ArduinoJson.h>

WiFiClient espClient;
PubSubClient mqtt(espClient);
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
Servo servoX, servoY;
TaskHandle_t sensorsTaskHandle;

// Current sensor readings
static float curLidar = 0;
static float curSmoke = 0;
static float curEco2  = 0;
static float curTvoc  = 0;
static float curTemp  = 0;
static float curRh    = 0;

static volatile bool servoMoved = false;

TaskHandle_t sensorsTaskHandle;
static int servoXAngle = 90;
static int servoYAngle = 90;
static volatile uint32_t lidarDueMs = 0;

void setServoAngles(int x, int y) {
    servoXAngle = constrain(x, 0, 180);
    servoYAngle = constrain(y, 0, 180);
    servoX.write(servoXAngle);
    servoY.write(servoYAngle);
    lidarDueMs = millis() + 100;
}

void wsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
             AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if(type != WS_EVT_DATA) return;
    String msg = String((char*)data).substring(0, len);
    if(msg == "X+") setServoAngles(servoXAngle + 1, servoYAngle);
    else if(msg == "X-") setServoAngles(servoXAngle - 1, servoYAngle);
    else if(msg == "Y+") setServoAngles(servoXAngle, servoYAngle + 1);
    else if(msg == "Y-") setServoAngles(servoXAngle, servoYAngle - 1);
    else return;
}

TaskHandle_t ledTaskHandle;
// Filter objects ...

unsigned long lastF2 = 0;
unsigned long lastHeartbeat = 0;

String hashPassword(const char *pwd) {
    unsigned char hash[32];
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts_ret(&ctx, 0);
    mbedtls_sha256_update_ret(&ctx, (const unsigned char*)pwd, strlen(pwd));
    mbedtls_sha256_finish_ret(&ctx, hash);
    mbedtls_sha256_free(&ctx);
    char hex[65];
    for (int i = 0; i < 32; ++i) sprintf(hex + i*2, "%02x", hash[i]);
    hex[64] = 0;
    return String(hex);
}

static float median3(float a, float b, float c) {
    if(a > b) std::swap(a, b);
    if(b > c) std::swap(b, c);
    if(a > b) std::swap(a, b);
    return b;
}

static void publishEvent(const char *name, float value) {
    char topic[64];
    snprintf(topic, sizeof(topic), "site/%s/event/%s", settings.siteName, name);
    String payload = String(value, 1);
    if(mqtt.connected()) {
        mqtt.publish(topic, payload.c_str());
    } else {
        bufferStore(topic, payload);
    }
}

static float readLidar() {
    if(Serial1.available()) {
        return Serial1.parseInt();
    }
    return curLidar;
}

static float readMQ2() {
    return analogRead(34);
}

static void readENS160(float &eco2, float &tvoc) {
    eco2 = 400 + (analogRead(35) % 1600); // placeholder
    tvoc = analogRead(36) % 600; // placeholder
}

static void readAHT21(float &t, float &rh) {
    t = 20.0 + (analogRead(37) % 100) / 10.0f; // placeholder
    rh = 40.0 + (analogRead(38) % 500) / 10.0f; // placeholder
}

static void setServoAngles(uint8_t x, uint8_t y) {
    servoX.write(x);
    servoY.write(y);
    servoMoved = true;
}

void sensorsTask(void*) {
    Serial1.begin(115200, SERIAL_8N1, 9, 10);

    const TickType_t lidarInt = pdMS_TO_TICKS(600000);
    const TickType_t sensorInt = pdMS_TO_TICKS(60000);

    TickType_t lastLidar = 0;
    TickType_t lastOther = 0;
    uint8_t clogCycles = 0;

    float mq2Buf[3] = {0};
    float eco2Buf[3] = {0};
    float tvocBuf[3] = {0};
    float tempBuf[3] = {0};
    float rhBuf[3] = {0};
    uint8_t idx = 0;

    for(;;) {
        TickType_t now = xTaskGetTickCount();

        if(servoMoved || now - lastLidar >= lidarInt) {
            if(servoMoved) {
                servoMoved = false;
                vTaskDelay(pdMS_TO_TICKS(100));
            }
            curLidar = readLidar();
            publishEvent("lidar", curLidar);
            if(curLidar < settings.clogMin) {
                if(++clogCycles >= settings.clogHold) {
                    publishEvent("clog", curLidar);
                    ledSetState(LedState::ALARM);
                    clogCycles = 0;
                }
            } else {
                clogCycles = 0;
            }
            lastLidar = now;
        }

        if(now - lastOther >= sensorInt) {
            float e, t, s, tv, h;
            s = readMQ2();
            readENS160(e, tv);
            readAHT21(t, h);

            mq2Buf[idx] = s;
            eco2Buf[idx] = e;
            tvocBuf[idx] = tv;
            tempBuf[idx] = t;
            rhBuf[idx] = h;
            idx = (idx + 1) % 3;

            float sMed = median3(mq2Buf[0], mq2Buf[1], mq2Buf[2]);
            float eMed = median3(eco2Buf[0], eco2Buf[1], eco2Buf[2]);
            float tvMed = median3(tvocBuf[0], tvocBuf[1], tvocBuf[2]);
            float tMed = median3(tempBuf[0], tempBuf[1], tempBuf[2]);
            float hMed = median3(rhBuf[0], rhBuf[1], rhBuf[2]);

            curSmoke = (curSmoke * 2 + sMed) / 3.0f;
            curEco2  = (curEco2 * 2 + eMed) / 3.0f;
            curTvoc  = (curTvoc * 2 + tvMed) / 3.0f;
            curTemp  = (curTemp * 2 + tMed) / 3.0f;
            curRh    = (curRh * 2 + hMed) / 3.0f;

            publishEvent("smoke", curSmoke);
            publishEvent("eco2", curEco2);
            publishEvent("tvoc", curTvoc);

            lastOther = now;
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
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
        doc["lidar"] = curLidar;
        doc["smoke"] = curSmoke;
        doc["eco2"] = curEco2;
        doc["tvoc"] = curTvoc;
        doc["temp"] = curTemp;
        doc["rh"]   = curRh;
        doc["x"] = servoX.read();
        doc["y"] = servoY.read();

        String out; serializeJson(doc, out);
        req->send(200, "application/json", out);
    });

    server.on("/api/servo", HTTP_POST, [](AsyncWebServerRequest *request){
        if(!request->authenticate(settings.uiUser, settings.uiPass))
            return request->requestAuthentication();
    }, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t, size_t){
        StaticJsonDocument<128> doc;
        if(deserializeJson(doc, data, len)) {
            request->send(400, "text/plain", "Bad JSON");
            return;
        }
        int x = doc["x"] | servoXAngle;
        int y = doc["y"] | servoYAngle;
        setServoAngles(x, y);
        request->send(200, "text/plain", "OK");
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
            static AsyncResponseStream *progress = nullptr;
            static size_t last = 0;
            if(!index){
                if(!request->authenticate(settings.uiUser, settings.uiPass)) return;
                if(!Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(Serial);
                progress = request->beginResponseStream("text/plain");
                last = 0;
            }
            if(!Update.hasError()){
                if(Update.write(data, len) != len) Update.printError(Serial);
            }
            size_t p = (index + len) * 100 / request->contentLength();
            if(progress && (p - last >= 5 || final)){
                progress->printf("%u\n", p);
                last = p;
            }
            if(final){
                if(Update.end(true)) Serial.printf("Update Success: %u bytes\n", index + len);
                else Update.printError(Serial);
                if(progress){
                    progress->addHeader("Connection", "close");
                    request->send(progress);
                    progress = nullptr;
                }
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
    xTaskCreatePinnedToCore(sensorsTask, "sensors", 4096, nullptr, 1, &sensorsTaskHandle, 1);
    servoX.attach(4);
    servoY.attach(5);
    servoX.write(servoXAngle);
    servoY.write(servoYAngle);
    xTaskCreatePinnedToCore(sensorsTask, "sensors", 4096, nullptr, 1, &sensorsTaskHandle, 1);
}

void loop() {
    mqtt.loop();
    ntpLoop();
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
