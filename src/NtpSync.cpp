#include "NtpSync.h"
#include <time.h>

static long tzOffset = 0;
static unsigned long lastSync = 0;
static const unsigned long syncInterval = 3600UL * 1000UL; // 1 hour

void ntpBegin(long gmtOffsetHours) {
    tzOffset = gmtOffsetHours * 3600;
    configTime(tzOffset, 0, "pool.ntp.org");
    lastSync = millis();
}

void ntpLoop() {
    if(millis() - lastSync >= syncInterval) {
        configTime(tzOffset, 0, "pool.ntp.org");
        lastSync = millis();
    }
}

time_t ntpNow() {
    time_t now;
    time(&now);
    return now;
}
