#pragma once
#include <Arduino.h>

struct CompassData {
    bool    valid;
    bool    calibrated;   // true si se aplicaron offsets con setCalibration()
    float   heading;      // grados magnéticos 0-360 (con offset aplicado)
    int16_t x;            // campo magnético eje X (raw, sin offset)
    int16_t y;            // campo magnético eje Y (raw, sin offset)
    int16_t z;            // campo magnético eje Z (raw)
    int16_t offX;         // offset hard-iron X aplicado
    int16_t offY;         // offset hard-iron Y aplicado
};

class CompassModule {
public:
    // address: 0x0D (por defecto QMC5883L)
    bool begin(uint8_t address = 0x0D);

    void update();

    const CompassData& getData() const { return _data; }

    // Ajuste de declinación magnética local en grados (ej: -7.5 para CDMX)
    void setDeclination(float degrees) { _declination = degrees; }

    // Aplicar offsets hard-iron conocidos.
    // Llamar en setup() después de begin() con los valores medidos previamente.
    void setCalibration(int16_t offX, int16_t offY);

private:
    void    writeReg(uint8_t reg, uint8_t val);
    uint8_t readReg(uint8_t reg);

    uint8_t     _addr;
    float       _declination;
    int16_t     _offX, _offY;
    CompassData _data;
};
