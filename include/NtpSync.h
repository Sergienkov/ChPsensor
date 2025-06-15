#ifndef NTPSYNC_H
#define NTPSYNC_H

void ntpBegin(long gmtOffsetHours = 0);
void ntpLoop();
time_t ntpNow();

#endif
