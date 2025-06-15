#ifndef LED_FSM_H
#define LED_FSM_H
#include <Arduino.h>

// Simple LED state machine used to indicate system status.
// A dedicated FreeRTOS task should call ``ledTask`` to blink
// according to the current state set via ``ledSetState``.

enum class LedState { NORMAL, ERROR, ALARM };

// Configure which GPIO pin controls the indicator LED.
void ledInit(uint8_t pin);

// Change the current LED behaviour.
void ledSetState(LedState s);

// RTOS task implementing the blinking patterns.  Intended to be
// run on a separate thread.
void ledTask(void*);

#endif
