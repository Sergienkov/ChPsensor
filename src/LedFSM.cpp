#include "LedFSM.h"

static uint8_t ledPin = 2;
static LedState current = LedState::NORMAL;

void ledInit(uint8_t pin) {
    ledPin = pin;
    pinMode(ledPin, OUTPUT);
}

void ledSetState(LedState s) {
    current = s;
}

void ledTask(void*) {
    const TickType_t normalOn = pdMS_TO_TICKS(100);
    const TickType_t normalOff = pdMS_TO_TICKS(9900);
    const TickType_t errorShort = pdMS_TO_TICKS(100);
    for(;;) {
        switch(current) {
            case LedState::NORMAL:
                digitalWrite(ledPin, HIGH);
                vTaskDelay(normalOn);
                digitalWrite(ledPin, LOW);
                vTaskDelay(normalOff);
                break;
            case LedState::ERROR:
                for(int i=0;i<2;i++) {
                    digitalWrite(ledPin, HIGH);
                    vTaskDelay(errorShort);
                    digitalWrite(ledPin, LOW);
                    vTaskDelay(errorShort);
                }
                vTaskDelay(pdMS_TO_TICKS(800));
                break;
            case LedState::ALARM:
                digitalWrite(ledPin, HIGH);
                vTaskDelay(pdMS_TO_TICKS(500));
                digitalWrite(ledPin, LOW);
                vTaskDelay(pdMS_TO_TICKS(500));
                break;
        }
    }
}
