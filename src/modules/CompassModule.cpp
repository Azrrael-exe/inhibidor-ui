#include "CompassModule.h"
#include "../logger.h"
#include <Wire.h>
#include <math.h>

// ─── Registros QMC5883L ───────────────────────────────────────────────────────
#define QMC5883L_REG_DATA_X_LSB  0x00
#define QMC5883L_REG_STATUS      0x06
#define QMC5883L_REG_CTRL1       0x09
#define QMC5883L_REG_CTRL2       0x0A
#define QMC5883L_REG_SET_RESET   0x0B
#define QMC5883L_REG_CHIP_ID     0x0D

// CTRL1: Mode=Continuous(01), ODR=200Hz(11), RNG=8G(01), OSR=512(00)
#define QMC5883L_CTRL1_CONTINUOUS 0x1D
#define QMC5883L_STATUS_DRDY      0x01

bool CompassModule::begin(uint8_t address) {
    _addr        = address;
    _declination = 0.0f;
    _offX        = 0;
    _offY        = 0;
    memset(&_data, 0, sizeof(_data));

    Wire.begin();

    if (readReg(QMC5883L_REG_CHIP_ID) != 0xFF) {
        LOG("Compass", "QMC5883L no encontrado");
        return false;
    }

    writeReg(QMC5883L_REG_SET_RESET, 0x01);
    writeReg(QMC5883L_REG_CTRL2,     0x00);
    writeReg(QMC5883L_REG_CTRL1,     QMC5883L_CTRL1_CONTINUOUS);

    LOG("Compass", "QMC5883L OK");
    return true;
}

void CompassModule::setCalibration(int16_t offX, int16_t offY) {
    _offX            = offX;
    _offY            = offY;
    _data.offX       = offX;
    _data.offY       = offY;
    _data.calibrated = true;
}

void CompassModule::update() {
    if (!(readReg(QMC5883L_REG_STATUS) & QMC5883L_STATUS_DRDY)) return;

    Wire.beginTransmission(_addr);
    Wire.write(QMC5883L_REG_DATA_X_LSB);
    Wire.endTransmission(false);
    Wire.requestFrom(_addr, (uint8_t)6);

    if (Wire.available() < 6) {
        _data.valid = false;
        return;
    }

    uint8_t xlsb = Wire.read();
    uint8_t xmsb = Wire.read();
    uint8_t ylsb = Wire.read();
    uint8_t ymsb = Wire.read();
    uint8_t zlsb = Wire.read();
    uint8_t zmsb = Wire.read();

    _data.x = (int16_t)((xmsb << 8) | xlsb);
    _data.y = (int16_t)((ymsb << 8) | ylsb);
    _data.z = (int16_t)((zmsb << 8) | zlsb);

    // Aplicar offset hard-iron al calcular el heading
    float cx = (float)(_data.x - _offX);
    float cy = (float)(_data.y - _offY);

    float heading = atan2(cy, cx) * 180.0f / M_PI;
    heading += _declination;
    if (heading < 0.0f)    heading += 360.0f;
    if (heading >= 360.0f) heading -= 360.0f;

    _data.heading = heading;
    _data.valid   = true;
}

void CompassModule::writeReg(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(_addr);
    Wire.write(reg);
    Wire.write(val);
    Wire.endTransmission();
}

uint8_t CompassModule::readReg(uint8_t reg) {
    Wire.beginTransmission(_addr);
    Wire.write(reg);
    Wire.endTransmission(false);
    Wire.requestFrom(_addr, (uint8_t)1);
    if (Wire.available()) return Wire.read();
    return 0xFF;
}
