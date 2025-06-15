#include "NtpSync.h"
#include <time.h>

void ntpBegin() {
    configTime(0, 0, "pool.ntp.org");
}
