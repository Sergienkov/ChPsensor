#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ESPAsyncWebServer.h>
#include <ESP32Servo.h>
#include "Config.h"
#include "LedFSM.h"

WiFiClient espClient;
PubSubClient mqtt(espClient);
AsyncWebServer server(80);
Servo servoX, servoY;

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

void setupWeb() {
    server.on("/api/settings", HTTP_POST, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", "OK");
    });
    server.on("/api/password", HTTP_POST, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", "OK");
    });
    server.begin();
}

void setup() {
    Serial.begin(115200);
    loadSettings();
    ledInit(2);
    xTaskCreate(ledTask, "led", 1024, nullptr, 1, &ledTaskHandle);
    connectWiFi();
    connectMQTT();
    setupWeb();
    servoX.attach(4);
    servoY.attach(5);
}

void loop() {
    mqtt.loop();
    delay(10);
}
