#include "RotorService.h"
#include "../logger.h"

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

void RotorService::stopAzimuth() {
    _txPack.clear();
    _txPack.addData(0xAA, (int16_t)0xA0);
    _txPack.write(*_serial);
    LOG("G5500", "CMD stop_az");
}

void RotorService::stopElevation() {
    _txPack.clear();
    _txPack.addData(0xBB, (int16_t)0xB0);
    _txPack.write(*_serial);
    LOG("G5500", "CMD stop_el");
}

void RotorService::update() {
    unsigned long now = millis();

    if (_pollState == POLL_IDLE) {
        if (_pendingCmd.pending) {
            // Do NOT log before write — ASCII on the G5500 serial bus corrupts LLP framing.
            _txPack.clear();
            if (_pendingCmd.hasAz) _txPack.addData(0xDA, (int16_t)(_pendingCmd.az * 10));
            if (_pendingCmd.hasEl) _txPack.addData(0xDB, (int16_t)(_pendingCmd.el * 10));
            _txPack.write(*_serial);
            // Log after write — gate passes because _pollState is still POLL_IDLE here.
            LOG_F("G5500", "CMD goto_az deg=", _pendingCmd.az);
            LOG_F("G5500", "CMD goto_el deg=", _pendingCmd.el);
            _pendingCmd.pending = false;
        }

        if (now - _lastPollMs >= _pollIntervalMs) {
            while (_serial->available()) _serial->read();  // flush stale bytes
            _txPack.clear();
            _txPack.addData(0xCC, (int16_t)0xC2);
            _txPack.write(*_serial);
            LOG("G5500", "CMD poll_status");  // log AFTER write, state still POLL_IDLE — gate passes
            _lastPollMs = now;
            _pollState  = POLL_SENT;
        }
        return;
    }

    // POLL_SENT: check if response has arrived (non-blocking).
    // Set a short timeout so readBytes() inside available() doesn't hold up loop().
    // NOTE: no logging here — _pollState == POLL_SENT means Serial TX is unsafe
    //       (ASCII bytes would reach the G5500 while it is sending its response).
    if (_serial->available()) {
        _serial->setTimeout(50);
        if (_rxPack.available(*_serial)) {
            if (_rxPack.hasKey(0xAB) && _rxPack.hasKey(0xBC)) {
                // getData returns uint16_t; G5500 angles are always non-negative so this is safe.
                _cachedStatus.azimuthAngle   = _rxPack.getData(0xAB) / 10.0f;
                _cachedStatus.elevationAngle = _rxPack.getData(0xBC) / 10.0f;
                _hasStatus = true;
            }
        }
        _pollState = POLL_IDLE;
        return;
    }

    // No bytes yet — check for timeout
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
