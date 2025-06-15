#include "SDP810.h"

bool SDP810::begin(TwoWire &w, uint8_t addr) {
    wire = &w;
    address = addr;
    wire->begin();
    wire->beginTransmission(address);
    wire->write(0x36);
    wire->write(0x15);
    return wire->endTransmission() == 0;
}

float SDP810::readPressure(float *temperature) {
    if(!wire) return NAN;
    wire->requestFrom((int)address, 9);
    uint8_t data[9];
    int i = 0;
    while(wire->available() && i < 9) {
        data[i++] = wire->read();
    }
    if(i != 9) return NAN;
    int16_t dpRaw = (data[0] << 8) | data[1];
    int16_t tempRaw = (data[3] << 8) | data[4];
    int16_t scale = (data[6] << 8) | data[7];
    if(scale == 0) return NAN;
    if(temperature) *temperature = tempRaw / 200.0f;
    return (float)dpRaw / (float)scale;
}
