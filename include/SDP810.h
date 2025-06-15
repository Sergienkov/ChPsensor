#ifndef SDP810_H
#define SDP810_H

#include <Arduino.h>
#include <Wire.h>

class SDP810 {
public:
    bool begin(TwoWire &w = Wire, uint8_t addr = 0x25);
    float readPressure(float *temperature = nullptr);
private:
    TwoWire* wire = nullptr;
    uint8_t address = 0x25;
};

#endif // SDP810_H
