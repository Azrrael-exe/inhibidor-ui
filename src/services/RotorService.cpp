#include "RotorService.h"

RotorService::RotorService(HardwareSerial* serial)
    : _serial(serial) {}

void RotorService::gotoAzimuth(float degrees) {
    _txPack.clear();
    _txPack.addData(0xDA, (int16_t)(degrees * 10));
    _txPack.write(*_serial);
}

void RotorService::gotoElevation(float degrees) {
    _txPack.clear();
    _txPack.addData(0xDB, (int16_t)(degrees * 10));
    _txPack.write(*_serial);
}

void RotorService::stopAzimuth() {
    _txPack.clear();
    _txPack.addData(0xAA, (int16_t)0xA0);
    _txPack.write(*_serial);
}

void RotorService::stopElevation() {
    _txPack.clear();
    _txPack.addData(0xBB, (int16_t)0xB0);
    _txPack.write(*_serial);
}

bool RotorService::readStatus(RotorStatus& out) {
    // Flush RX buffer
    while (_serial->available()) _serial->read();

    // Send feedback poll
    _txPack.clear();
    _txPack.addData(0xCC, (int16_t)0xC2);
    _txPack.write(*_serial);

    // available() blocks via Stream::readBytes which respects setTimeout.
    // The 200ms timeout applies once bytes start arriving.
    _serial->setTimeout(200);
    // inp_buffer is overwritten by available() on success — clear() is not needed for RX.
    if (!_rxPack.available(*_serial)) return false;
    if (!_rxPack.hasKey(0xAB) || !_rxPack.hasKey(0xBC)) return false;

    out.azimuthAngle   = _rxPack.getData(0xAB) / 10.0f;
    out.elevationAngle = _rxPack.getData(0xBC) / 10.0f;
    return true;
}
