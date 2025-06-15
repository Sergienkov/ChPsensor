#ifndef LED_FSM_H
#define LED_FSM_H
#include <Arduino.h>

enum class LedState { NORMAL, ERROR, ALARM };

void ledInit(uint8_t pin);
void ledSetState(LedState s);
void ledTask(void*);

#endif
