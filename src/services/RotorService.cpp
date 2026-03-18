#include "RotorService.h"

// Set to 1 to enable G5500 debug logs on Serial.
// Keep at 0 in production — logs share the Serial port with G5500 binary frames.
#define ROTOR_DEBUG 0

#if ROTOR_DEBUG
#  define ROTOR_LOG(msg)       Serial.print(F("[G5500] ")); Serial.println(F(msg))
#  define ROTOR_LOG_F(msg, v)  Serial.print(F("[G5500] ")); Serial.print(F(msg)); Serial.println(v)
#else
#  define ROTOR_LOG(msg)
#  define ROTOR_LOG_F(msg, v)
#endif

RotorService::RotorService(HardwareSerial* serial)
    : _serial(serial) {}

void RotorService::gotoAzimuth(float degrees) {
    ROTOR_LOG_F("CMD goto_az deg=", degrees);
    // clear() before addData is explicit defensive init; write() also calls clear() internally.
    _txPack.clear();
    _txPack.addData(0xDA, (int16_t)(degrees * 10));
    _txPack.write(*_serial);
}

void RotorService::gotoElevation(float degrees) {
    ROTOR_LOG_F("CMD goto_el deg=", degrees);
    _txPack.clear();
    _txPack.addData(0xDB, (int16_t)(degrees * 10));
    _txPack.write(*_serial);
}

void RotorService::stopAzimuth() {
    ROTOR_LOG("CMD stop_az");
    _txPack.clear();
    _txPack.addData(0xAA, (int16_t)0xA0);
    _txPack.write(*_serial);
}

void RotorService::stopElevation() {
    ROTOR_LOG("CMD stop_el");
    _txPack.clear();
    _txPack.addData(0xBB, (int16_t)0xB0);
    _txPack.write(*_serial);
}

bool RotorService::readStatus(RotorStatus& out) {
    // Flush RX buffer
    while (_serial->available()) _serial->read();

    // Send feedback poll — log before sending, not during receive window
    ROTOR_LOG("CMD poll_status");
    _txPack.clear();
    _txPack.addData(0xCC, (int16_t)0xC2);
    _txPack.write(*_serial);

    // available() blocks via Stream::readBytes which respects setTimeout.
    // The 200ms timeout applies once bytes start arriving.
    _serial->setTimeout(200);
    // inp_buffer is overwritten by available() on success — clear() is not needed for RX.
    if (!_rxPack.available(*_serial)) {
        ROTOR_LOG("STATUS timeout");
        return false;
    }
    if (!_rxPack.hasKey(0xAB) || !_rxPack.hasKey(0xBC)) {
        ROTOR_LOG("STATUS missing keys");
        return false;
    }

    // getData returns uint16_t; G5500 angles are always non-negative so this is safe.
    out.azimuthAngle   = _rxPack.getData(0xAB) / 10.0f;
    out.elevationAngle = _rxPack.getData(0xBC) / 10.0f;

#if ROTOR_DEBUG
    Serial.print(F("[G5500] STATUS az="));
    Serial.print(out.azimuthAngle);
    Serial.print(F(" el="));
    Serial.println(out.elevationAngle);
#endif

    return true;
}
