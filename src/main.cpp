#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ESPAsyncWebServer.h>
#include <ESP32Servo.h>
#include <Update.h>
#include <Wire.h>
#include "Config.h"
#include "mbedtls/sha256.h"
#include "mbedtls/base64.h"
#include <vector>
#include "LedFSM.h"
#include "NtpSync.h"
#include "MsgBuffer.h"
#include "Debug.h"
#include "Filter.h"
#include <ArduinoJson.h>
#include <MQUnifiedsensor.h>
#include "SparkFun_ENS160.h"
#include <Adafruit_AHTX0.h>
#include "SDP810.h"

WiFiClient espClient;
PubSubClient mqtt(espClient);
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
Servo servoX, servoY;

TaskHandle_t sensorsTaskHandle;
TaskHandle_t servoTaskHandle;

static volatile int servoXAngle = 90;
static volatile int servoYAngle = 90;
// Timestamp when the next lidar reading should occur.  Set by the
// servo task whenever the head is moved.
static volatile uint32_t lidarDueMs = 0;

struct ServoCmd {
    bool absolute;
    int x;
    int y;
};

// Queue used to pass servo movement commands from the web interface
// to the dedicated servo task.
static QueueHandle_t servoQueue;

void setServoAngles(int x, int y) {
    ServoCmd cmd{true, x, y};
    if(servoQueue) xQueueSend(servoQueue, &cmd, 0);
}

void wsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
             AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if(type != WS_EVT_DATA) return;
    String msg = String((char*)data).substring(0, len);
    ServoCmd cmd{false,0,0};
    if(msg == "X+") cmd.x = 1;
    else if(msg == "X-") cmd.x = -1;
    else if(msg == "Y+") cmd.y = 1;
    else if(msg == "Y-") cmd.y = -1;
    else return;
    if(servoQueue) xQueueSend(servoQueue, &cmd, 0);
}

TaskHandle_t ledTaskHandle;
// Filter objects and sensor instances
MQUnifiedsensor mq2("ESP32", 3.3, 12, 34, "MQ-2");
SparkFun_ENS160 ens160;
Adafruit_AHTX0 aht21;
SDP810 sdp810;

SMAFilter<float, 5> mq2Filter;
SMAFilter<float, 5> eco2Filter;
SMAFilter<float, 5> tvocFilter;
SMAFilter<float, 5> tempFilter;
SMAFilter<float, 5> rhFilter;
SMAFilter<float, 5> presFilter;

float lastMq2 = 0;
float lastEco2 = 0;
float lastTvoc = 0;
float lastTemp = 0;
float lastRh = 0;
float lastLidar = 0;
float lastPressure = 0;
unsigned long lastHeartbeat = 0;

// Current alarm state for each sensor used to implement hysteresis
static bool lidarAlarm = false;
static bool smokeAlarm = false;
static bool eco2Alarm  = false;
static bool tvocAlarm  = false;
static bool presAlarm  = false;

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

bool checkAuth(AsyncWebServerRequest *req) {
    if(!req->hasHeader("Authorization")) return false;
    String hdr = req->header("Authorization");
    if(!hdr.startsWith("Basic ")) return false;
    String enc = hdr.substring(6);
    size_t enclen = enc.length();
    size_t outlen = 0;
    std::vector<unsigned char> out(enclen * 3 / 4 + 1);
    if(mbedtls_base64_decode(out.data(), out.size(), &outlen,
                             (const unsigned char*)enc.c_str(), enclen) != 0)
        return false;
    out[outlen] = 0;
    String decoded = String((char*)out.data());
    int sep = decoded.indexOf(':');
    if(sep <= 0) return false;
    String user = decoded.substring(0, sep);
    String pass = decoded.substring(sep + 1);
    if(user != settings.uiUser) return false;
    String hashed = hashPassword(pass.c_str());
    return hashed == settings.uiPass;
}

// Connect to Wi-Fi using credentials from Settings.  When no
// credentials are configured the board starts in AP mode so that
// the user can provide them via the web interface.
void connectWiFi() {
    if(strlen(settings.wifiSSID) == 0) {
        WiFi.softAP("start", "starttrats");
        ledSetState(LedState::ERROR);
        debugPublish("WiFi AP mode");
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
        debugPublish("WiFi connect fail");
    } else {
        debugPublish("WiFi connected");
    }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
    // handle incoming messages
}

// Establish connection to the MQTT broker defined in Settings and
// publish the online status.  A Last Will message is registered so
// that clients are notified when the device goes offline.
void connectMQTT() {
    mqtt.setServer(settings.mqttHost, settings.mqttPort);
    mqtt.setCallback(mqttCallback);
    char willTopic[64];
    snprintf(willTopic, sizeof(willTopic), "site/%s/status", settings.siteName);
    String clientId = String("client-") + String((uint32_t)ESP.getEfuseMac(), HEX);
    if(!mqtt.connect(clientId.c_str(), settings.mqttUser, settings.mqttPass,
                     willTopic, settings.mqttQos, true, "offline")) {
        ledSetState(LedState::ERROR);
        debugPublish("MQTT connect fail");
    } else {
        ledSetState(LedState::NORMAL);
        debugPublish("MQTT connected");
        mqtt.publish(willTopic, "online", settings.mqttQos, true);
    }
}

// -------- Sensor stubs and publishing helpers --------

float readMQ2() {
    mq2.update();
    return mq2.readSensor();
}

float readEco2() {
    if(ens160.checkDataStatus())
        return ens160.getECO2();
    return lastEco2;
}

float readTvoc() {
    if(ens160.checkDataStatus())
        return ens160.getTVOC();
    return lastTvoc;
}

float readTemp() {
    sensors_event_t h, t;
    aht21.getEvent(&h, &t);
    return t.temperature;
}

float readRh() {
    sensors_event_t h, t;
    aht21.getEvent(&h, &t);
    return h.relative_humidity;
}

float readPressure() {
    return sdp810.readPressure();
}

void publishEvent(const char* name, float value) {
    char topic[64];
    snprintf(topic, sizeof(topic), "site/%s/event/%s", settings.siteName, name);
    char payload[32];
    snprintf(payload, sizeof(payload), "%.2f", value);
    if(mqtt.connected()) mqtt.publish(topic, payload, settings.mqttQos, false);
    else bufferStore(topic, payload);
    char dbg[64];
    snprintf(dbg, sizeof(dbg), "event %s %.2f", name, value);
    debugPublish(dbg);
}

void publishHeartbeat() {
    char topic[64];
    snprintf(topic, sizeof(topic), "site/%s/heartbeat", settings.siteName);
    StaticJsonDocument<256> doc;
    doc["smoke"] = lastMq2;
    doc["lidar"] = lastLidar;
    doc["pressure"] = lastPressure;
    doc["eco2"] = lastEco2;
    doc["tvoc"] = lastTvoc;
    doc["temp"] = lastTemp;
    doc["rh"] = lastRh;
    doc["heap"] = ESP.getFreeHeap();
    String out; serializeJson(doc, out);
    if(mqtt.connected()) mqtt.publish(topic, out.c_str(), settings.mqttQos, false);
    else bufferStore(topic, out);
    debugPublish("heartbeat");
}

// Apply hysteresis to sensor thresholds. When the current alarm state is
// false, readings must exceed the threshold by 5% to trigger the alarm.
// Once in alarm state, values must return inside the opposite 5% band to clear.
void checkThreshold(const char *name, float value,
                    float min, float max, bool &state) {
    // Если порог отрицательный, полоса смещается в обратную сторону.
    // Например, при min < 0 включение: onLow = min * 1.05, выключение: offLow = min * 0.95
    float onLow  = min * (min < 0 ? 1.05f : 0.95f);
    float onHigh = max * (max < 0 ? 0.95f : 1.05f);
    float offLow  = min * (min < 0 ? 0.95f : 1.05f);
    float offHigh = max * (max < 0 ? 1.05f : 0.95f);
    if(!state) {
        if(value < onLow || value > onHigh) {
            state = true;
            publishEvent(name, value);
        }
    } else {
        if(value > offLow && value < offHigh) {
            state = false;
            publishEvent(name, value);
        }
    }
}

void checkSensors() {
    mq2Filter.add(readMQ2());
    eco2Filter.add(readEco2());
    tvocFilter.add(readTvoc());
    tempFilter.add(readTemp());
    rhFilter.add(readRh());
    presFilter.add(readPressure());

    lastMq2 = mq2Filter.average();
    lastEco2 = eco2Filter.average();
    lastTvoc = tvocFilter.average();
    lastTemp = tempFilter.average();
    lastRh = rhFilter.average();
    lastPressure = presFilter.average();

    // Check each sensor with 5% hysteresis before publishing alarm events
    checkThreshold("smoke", lastMq2,
                   settings.thr.smokeMin, settings.thr.smokeMax, smokeAlarm);
    checkThreshold("eco2", lastEco2,
                   settings.thr.eco2Min, settings.thr.eco2Max, eco2Alarm);
    checkThreshold("tvoc", lastTvoc,
                   settings.thr.tvocMin, settings.thr.tvocMax, tvocAlarm);
    checkThreshold("pressure", lastPressure,
                   settings.thr.presMin, settings.thr.presMax, presAlarm);
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
    auto p = thr.createNestedObject("pressure");
    p["min"] = settings.thr.presMin; p["max"] = settings.thr.presMax;
    auto clog = doc.createNestedObject("clog");
    clog["clogMin"] = settings.clogMin;
    clog["clogHold"] = settings.clogHold;
    doc["debugEnable"] = settings.debugEnable;
    doc["uiUser"] = settings.uiUser;
    return doc;
}

void handleSettingsPost(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t, size_t) {
    if(!checkAuth(request)) { request->requestAuthentication(); return; }
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
        JsonObject p = thr["pressure"]; if(!p.isNull()) { settings.thr.presMin = p["min"] | settings.thr.presMin; settings.thr.presMax = p["max"] | settings.thr.presMax; }
    }
    JsonObject clog = doc["clog"]; if(!clog.isNull()) { settings.clogMin = clog["clogMin"] | settings.clogMin; settings.clogHold = clog["clogHold"] | settings.clogHold; }
    settings.debugEnable = doc["debugEnable"] | settings.debugEnable;
    const char *user = doc["uiUser"] | settings.uiUser; strlcpy(settings.uiUser, user, sizeof(settings.uiUser));
    saveSettings();
    request->send(200, "text/plain", "OK");
}

void handlePasswordPost(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t, size_t) {
    if(!checkAuth(request)) { request->requestAuthentication(); return; }
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
    // Read distance value from SF11c via UART1
    while(Serial1.available()) Serial1.read();
    Serial1.setTimeout(50);
    String resp = Serial1.readStringUntil('\n');
    return resp.toFloat();
}

// Task controlling the two servos that aim the lidar sensor. After
// moving to the requested position a measurement is triggered via
// ``lidarDueMs``.
void servoTask(void*) {
    servoX.attach(4);
    servoY.attach(5);
    servoX.write(servoXAngle);
    servoY.write(servoYAngle);
    ServoCmd cmd;
    for(;;) {
        if(xQueueReceive(servoQueue, &cmd, portMAX_DELAY)) {
            if(cmd.absolute) {
                servoXAngle = constrain(cmd.x, 0, 180);
                servoYAngle = constrain(cmd.y, 0, 180);
            } else {
                servoXAngle = constrain(servoXAngle + cmd.x, 0, 180);
                servoYAngle = constrain(servoYAngle + cmd.y, 0, 180);
            }
            servoX.write(servoXAngle);
            servoY.write(servoYAngle);
            char buf[32];
            snprintf(buf, sizeof(buf), "servo %d %d", servoXAngle, servoYAngle);
            debugPublish(buf);
            vTaskDelay(pdMS_TO_TICKS(100));
            lidarDueMs = millis();
        }
    }
}

// Background task that periodically samples all sensors and triggers
// lidar measurements.  Gas and pressure sensors are checked once a
// minute while the lidar runs every ten minutes or when requested by
// the servo task.
void sensorsTask(void*) {
    const TickType_t delayStep = pdMS_TO_TICKS(100);    // task loop period
    const uint32_t lidarPeriod = 600000;                // 10 min between lidar scans
    Wire.begin(8, 3);
    Serial1.begin(115200, SERIAL_8N1, 9, 10);
    mq2.init();
    mq2.setRegressionMethod(1);
    mq2.setA(574.25); mq2.setB(-2.222);
    mq2.setRL(5);
    mq2.calibrate(9.83);
    ens160.begin();
    aht21.begin();
    sdp810.begin();
    uint32_t lastLidarTs = 0;          // last time the lidar was triggered
    uint8_t clogCnt = 0;               // consecutive readings below clogMin
    uint32_t lastSensors = 0;          // last time environmental sensors were read
    for(;;) {
        uint32_t now = millis();
        if(now - lastSensors >= 60000) {       // update environmental sensors once a minute
            checkSensors();
            lastSensors = now;
        }
        if((now - lastLidarTs >= lidarPeriod) || (lidarDueMs && now >= lidarDueMs)) {
            float dist = readLidar();
            lastLidar = dist;
            lastLidarTs = now;
            // Apply hysteresis when checking distance limits
            checkThreshold("lidar", dist,
                           settings.thr.lidarMin, settings.thr.lidarMax, lidarAlarm);
            lidarDueMs = 0;
            if(dist < settings.clogMin) {
                if(++clogCnt >= settings.clogHold) {
                    if(clogCnt == settings.clogHold) {
                        publishEvent("clog", dist);
                    }
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
        if(!checkAuth(req)) return req->requestAuthentication();
        StaticJsonDocument<512> doc = buildSettingsJson();
        String out; serializeJson(doc, out);
        req->send(200, "application/json", out);
    });
    server.on("/api/settings", HTTP_POST, [](AsyncWebServerRequest *request){
        if(!checkAuth(request)) return request->requestAuthentication();
    }, NULL, handleSettingsPost);
    server.on("/api/password", HTTP_POST, [](AsyncWebServerRequest *request){
        if(!checkAuth(request)) return request->requestAuthentication();
    }, NULL, handlePasswordPost);
    server.on("/api/live", HTTP_GET, [](AsyncWebServerRequest *req){
        StaticJsonDocument<256> doc;
        doc["lidar"] = lastLidar;
        doc["smoke"] = lastMq2;
        doc["eco2"] = lastEco2;
        doc["tvoc"] = lastTvoc;
        doc["pressure"] = lastPressure;
        doc["temp"] = lastTemp;
        doc["rh"] = lastRh;
        doc["x"] = servoXAngle;
        doc["y"] = servoYAngle;
        String out; serializeJson(doc, out);
        req->send(200, "application/json", out);
    });

    server.on("/api/servo", HTTP_POST, [](AsyncWebServerRequest *request){
        if(!checkAuth(request))
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
            if(!checkAuth(request))
                return request->requestAuthentication();
            bool ok = !Update.hasError();
            AsyncWebServerResponse *resp = request->beginResponse(200, "text/plain", ok ? "OK" : "FAIL");
            resp->addHeader("Connection", "close");
            resp->addHeader("X-Site-Name", settings.siteName);
            request->send(resp);
            if(ok) ESP.restart();
        },
        [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
            static AsyncResponseStream *progress = nullptr;
            static size_t last = 0;
            if(!index){
                if(!checkAuth(request)) return;
                if(!Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(Serial);
                progress = request->beginResponseStream("text/plain");
                progress->addHeader("X-Site-Name", settings.siteName);
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
                    progress->addHeader("X-Site-Name", settings.siteName);
                    request->send(progress);
                    progress = nullptr;
                }
            }
        });
    ws.onEvent(wsEvent);
    server.addHandler(&ws);
    server.begin();
}

void setup() {
    Serial.begin(115200);
    loadSettings();
    ledInit(2);
    xTaskCreate(ledTask, "led", 1024, nullptr, 1, &ledTaskHandle);
    connectWiFi();                       // establish network connection
    ntpBegin();                          // start periodic NTP time sync
    connectMQTT();                       // connect to broker and publish status
    bufferInit();
    setupWeb();
    servoQueue = xQueueCreate(4, sizeof(ServoCmd));
    xTaskCreatePinnedToCore(servoTask, "servo", 2048, nullptr, 1, &servoTaskHandle, 1);
    xTaskCreatePinnedToCore(sensorsTask, "sensors", 4096, nullptr, 1, &sensorsTaskHandle, 1);
}

void loop() {
    static bool wasConnected = false;
    mqtt.loop();                       // maintain MQTT connection
    ntpLoop();                         // refresh NTP time if needed
    if(!mqtt.connected()) {
        connectMQTT();
    }
    bool connected = mqtt.connected();
    if(connected && !wasConnected) {
        bufferFlush();
    }
    wasConnected = connected;

    unsigned long now = millis();
    // Send a summary of sensor readings every hour (3600000 ms)
    if(now - lastHeartbeat >= 3600000) {
        publishHeartbeat();
        lastHeartbeat = now;
    }
    delay(10);
}
