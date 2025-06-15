#ifndef MSGBUFFER_H
#define MSGBUFFER_H
#include <Arduino.h>
#include <LittleFS.h>

// Simple persistent buffer for MQTT messages when the connection
// is down.  Messages are stored in LittleFS and resent on reconnect.

// Mount the filesystem used for buffering.
void bufferInit();

// Append a topic/payload pair to the buffer file.  Called when
// the MQTT client is offline.
void bufferStore(const String& topic, const String& payload);

// Send all buffered messages and clear the file.  Should be called
// once after MQTT reconnects.
void bufferFlush();

#endif
