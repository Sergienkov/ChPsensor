#ifndef MSGBUFFER_H
#define MSGBUFFER_H
#include <Arduino.h>
#include <LittleFS.h>

void bufferInit();
void bufferStore(const String& topic, const String& payload);
void bufferFlush();

#endif
