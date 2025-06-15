#ifndef NTPSYNC_H
#define NTPSYNC_H

// Initialise NTP and set the system time zone offset in hours.
// Should be called once during start-up.
void ntpBegin(long gmtOffsetHours = 0);

// Periodically resynchronise the clock.  Call from the main loop.
void ntpLoop();

// Return the current UNIX timestamp according to the last NTP sync.
time_t ntpNow();

#endif
