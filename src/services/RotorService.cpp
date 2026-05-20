#include "RotorService.h"
#include "../logger.h"
#include <math.h>

RotorService::RotorService(HardwareSerial* serial, unsigned long pollIntervalMs)
    : _serial(serial)
    , _pollState(POLL_IDLE)
    , _lastPollMs(0)
    , _pollIntervalMs(pollIntervalMs)
    , _cachedStatus({0.0f, 0.0f})
    , _hasStatus(false)
    , _pendingCmd({false, 0.0f, false, 0.0f, false}) {}

void RotorService::enqueuePosition(bool hasAz, float az, bool hasEl, float el) {
    _pendingCmd = { hasAz, az, hasEl, el, true };
}

void RotorService::emergencyKill() {
    while (_serial->available()) { _serial->read(); }
    _pollState = POLL_IDLE;
    _txPack.clear();
    _txPack.addData(0xFF, (int16_t)0x01);
    _txPack.write(*_serial);
    _pendingCmd.pending = false;
}

void RotorService::home() {
    while (_serial->available()) { _serial->read(); }
    _pollState = POLL_IDLE;
    _txPack.clear();
    _txPack.addData(0xFF, (int16_t)0x02);
    _txPack.write(*_serial);
    _pendingCmd.pending = false;
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

void RotorService::update() {
    unsigned long now = millis();

    if (_pollState == POLL_IDLE) {
        if (_pendingCmd.pending) {
            _txPack.clear();
            if (_pendingCmd.hasAz) {
                _txPack.addData(0xDA, (int16_t)(_pendingCmd.az * 10));
            }
            if (_pendingCmd.hasEl) {
                _txPack.addData(0xDB, (int16_t)(_pendingCmd.el * 10));
            }
            _txPack.write(*_serial);
            _pendingCmd.pending = false;
            _lastPollMs = now;  // delay next poll — avoid back-to-back frame with goto cmd
        }

        if (now - _lastPollMs >= _pollIntervalMs) {
            while (_serial->available()) { _serial->read(); }

            _txPack.clear();
            _txPack.addData(0xCC, (int16_t)0xC2);
            _txPack.write(*_serial);
            _lastPollMs = now;
            _pollState  = POLL_SENT;
        }
        return;
    }

    if (_serial->available()) {
        _serial->setTimeout(5);
        if (_rxPack.available(*_serial)) {
            if (_rxPack.hasKey(0xAB) && _rxPack.hasKey(0xBC)) {
                // Rotor envía ángulos como int16_t (rango físico ~[-223, +223] para az).
                // Casteamos para recuperar el signo y normalizamos azimuth a [0, 360).
                float azSigned = (int16_t)_rxPack.getData(0xAB) / 10.0f;
                _cachedStatus.azimuthAngle   = fmodf(fmodf(azSigned, 360.0f) + 360.0f, 360.0f);
                _cachedStatus.elevationAngle = (int16_t)_rxPack.getData(0xBC) / 10.0f;
                if (!_hasStatus) {
                    LOG("G5500", "Connected");
                }
                _hasStatus = true;
            }
        }
        _pollState = POLL_IDLE;
        return;
    }

    if (now - _lastPollMs >= POLL_TIMEOUT_MS) {
        _pollState = POLL_IDLE;
    }
}

bool RotorService::hasStatus() const {
    return _hasStatus;
}

const RotorStatus& RotorService::getStatus() const {
    return _cachedStatus;
}

bool RotorService::isSerialFree() const {
    return _pollState == POLL_IDLE;
}
