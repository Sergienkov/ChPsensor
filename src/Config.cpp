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
    prefs.end();
}
