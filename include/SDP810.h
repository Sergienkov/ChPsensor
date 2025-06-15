#ifndef SDP810_H
#define SDP810_H

#include <Arduino.h>
#include <Wire.h>

// Lightweight driver for the Sensirion SDP810 differential pressure sensor.
class SDP810 {
public:
    // Initialise the sensor on the provided I²C bus.  Returns true when the
    // sensor acknowledges the start measurement command.
    bool begin(TwoWire &w = Wire, uint8_t addr = 0x25);

    // Read the differential pressure in Pascals.  Optionally returns the
    // measured temperature via the pointer argument.
    float readPressure(float *temperature = nullptr);
private:
    TwoWire* wire = nullptr;
    uint8_t address = 0x25;
};

#endif // SDP810_H
