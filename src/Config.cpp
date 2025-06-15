#include "Config.h"
#include <Preferences.h>

Settings settings;
Preferences prefs;

void loadSettings() {
    prefs.begin("config", true);
    prefs.getString("siteName", settings.siteName, sizeof(settings.siteName));
    prefs.getString("wifiSSID", settings.wifiSSID, sizeof(settings.wifiSSID));
    prefs.getString("wifiPass", settings.wifiPass, sizeof(settings.wifiPass));
    prefs.getString("mqttHost", settings.mqttHost, sizeof(settings.mqttHost));
    settings.mqttPort = prefs.getUShort("mqttPort", settings.mqttPort);
    prefs.getString("mqttUser", settings.mqttUser, sizeof(settings.mqttUser));
    prefs.getString("mqttPass", settings.mqttPass, sizeof(settings.mqttPass));
    settings.mqttQos = prefs.getUChar("mqttQos", settings.mqttQos);
    prefs.getString("uiUser", settings.uiUser, sizeof(settings.uiUser));
    prefs.getString("uiPass", settings.uiPass, sizeof(settings.uiPass));
    settings.debugEnable = prefs.getBool("debugEnable", settings.debugEnable);
    settings.thr.lidarMin = prefs.getFloat("lidarMin", settings.thr.lidarMin);
    settings.thr.lidarMax = prefs.getFloat("lidarMax", settings.thr.lidarMax);
    settings.thr.smokeMin = prefs.getFloat("smokeMin", settings.thr.smokeMin);
    settings.thr.smokeMax = prefs.getFloat("smokeMax", settings.thr.smokeMax);
    settings.thr.eco2Min  = prefs.getFloat("eco2Min", settings.thr.eco2Min);
    settings.thr.eco2Max  = prefs.getFloat("eco2Max", settings.thr.eco2Max);
    settings.thr.tvocMin  = prefs.getFloat("tvocMin", settings.thr.tvocMin);
    settings.thr.tvocMax  = prefs.getFloat("tvocMax", settings.thr.tvocMax);
    settings.thr.presMin  = prefs.getFloat("presMin", settings.thr.presMin);
    settings.thr.presMax  = prefs.getFloat("presMax", settings.thr.presMax);
    settings.clogMin = prefs.getUShort("clogMin", settings.clogMin);
    settings.clogHold = prefs.getUChar("clogHold", settings.clogHold);
    prefs.end();
}

void saveSettings() {
    prefs.begin("config", false);
    prefs.putString("siteName", settings.siteName);
    prefs.putString("wifiSSID", settings.wifiSSID);
    prefs.putString("wifiPass", settings.wifiPass);
    prefs.putString("mqttHost", settings.mqttHost);
    prefs.putUShort("mqttPort", settings.mqttPort);
    prefs.putString("mqttUser", settings.mqttUser);
    prefs.putString("mqttPass", settings.mqttPass);
    prefs.putUChar("mqttQos", settings.mqttQos);
    prefs.putString("uiUser", settings.uiUser);
    prefs.putString("uiPass", settings.uiPass);
    prefs.putBool("debugEnable", settings.debugEnable);
    prefs.putFloat("lidarMin", settings.thr.lidarMin);
    prefs.putFloat("lidarMax", settings.thr.lidarMax);
    prefs.putFloat("smokeMin", settings.thr.smokeMin);
    prefs.putFloat("smokeMax", settings.thr.smokeMax);
    prefs.putFloat("eco2Min", settings.thr.eco2Min);
    prefs.putFloat("eco2Max", settings.thr.eco2Max);
    prefs.putFloat("tvocMin", settings.thr.tvocMin);
    prefs.putFloat("tvocMax", settings.thr.tvocMax);
    prefs.putFloat("presMin", settings.thr.presMin);
    prefs.putFloat("presMax", settings.thr.presMax);
    prefs.putUShort("clogMin", settings.clogMin);
    prefs.putUChar("clogHold", settings.clogHold);
    prefs.end();
}
