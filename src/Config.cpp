#include "Config.h"
#include <Preferences.h>
#include "mbedtls/sha256.h"

static bool isHexHash(const char *p) {
    if(strlen(p) != 64) return false;
    for(size_t i = 0; i < 64; i++) {
        char c = p[i];
        if(!isxdigit(c)) return false;
    }
    return true;
}

void hashPassword(const char *input, char output[65]) {
    unsigned char hash[32];
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);
    mbedtls_sha256_update(&ctx, (const unsigned char *)input, strlen(input));
    mbedtls_sha256_finish(&ctx, hash);
    mbedtls_sha256_free(&ctx);
    for(int i = 0; i < 32; i++) sprintf(output + i*2, "%02x", hash[i]);
    output[64] = '\0';
}

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
    char hashed[65];
    if(!isHexHash(settings.uiPass)) {
        hashPassword(settings.uiPass, hashed);
        strlcpy(settings.uiPass, hashed, sizeof(settings.uiPass));
    }
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
    prefs.putUShort("clogMin", settings.clogMin);
    prefs.putUChar("clogHold", settings.clogHold);
    prefs.end();
}
