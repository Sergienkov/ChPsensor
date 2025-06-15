#ifndef MSGBUFFER_H
#define MSGBUFFER_H
#include <Arduino.h>
#include <LittleFS.h>

class PubSubClient;

void bufferInit();
void bufferStore(const String& topic, const String& payload);
void bufferFlush(PubSubClient &client);

#endif
