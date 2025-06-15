#ifndef DEBUG_H
#define DEBUG_H
#include <Arduino.h>

// Publish a formatted debug message to MQTT.
// Debug output is sent only when ``settings.debugEnable`` is true.
// Messages are published to ``site/<SiteName>/debug`` using the
// configured QoS level.
void debugPublish(const String& msg);

#endif // DEBUG_H
