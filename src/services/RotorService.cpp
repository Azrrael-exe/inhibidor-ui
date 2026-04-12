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

void RotorService::emergencyKill() {
    _txPack.clear();
    _txPack.addData(0xFF, (int16_t)0x01);
    _txPack.write(*_serial);
    _pendingCmd.pending = false; // Cancel any pending goto
    LOG("G5500", "CMD emergency_kill");
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
            _txPack.clear();
            if (_pendingCmd.hasAz) {
                _txPack.addData(0xDA, (int16_t)(_pendingCmd.az * 10));
                LOG_F("G5500", "TX goto_az deg=", _pendingCmd.az);
            }
            if (_pendingCmd.hasEl) {
                _txPack.addData(0xDB, (int16_t)(_pendingCmd.el * 10));
                LOG_F("G5500", "TX goto_el deg=", _pendingCmd.el);
            }
            _txPack.write(*_serial);
            _pendingCmd.pending = false;
            _lastPollMs = now;  // delay next poll — avoid back-to-back frame with goto cmd
        }

        if (now - _lastPollMs >= _pollIntervalMs) {
            uint8_t flushed = 0;
            while (_serial->available()) { _serial->read(); flushed++; }
            if (flushed > 0) LOG_F("G5500", "POLL flushed stale bytes=", flushed);

            _txPack.clear();
            _txPack.addData(0xCC, (int16_t)0xC2);
            _txPack.write(*_serial);
            _lastPollMs = now;
            _pollState  = POLL_SENT;
        }
        return;
    }

    // POLL_SENT: Serial and Serial2 are independent buses — logging is safe here.
    if (_serial->available()) {
        _serial->setTimeout(50);
        if (_rxPack.available(*_serial)) {
            if (_rxPack.hasKey(0xAB) && _rxPack.hasKey(0xBC)) {
                _cachedStatus.azimuthAngle   = _rxPack.getData(0xAB) / 10.0f;
                _cachedStatus.elevationAngle = _rxPack.getData(0xBC) / 10.0f;
                _hasStatus = true;
                LOG_F("G5500", "RX az_deg=", _cachedStatus.azimuthAngle);
                LOG_F("G5500", "RX el_deg=", _cachedStatus.elevationAngle);
            } else {
                LOG("G5500", "RX frame missing key 0xAB or 0xBC");
            }
        } else {
            LOG("G5500", "RX frame FAILED (bad checksum or framing)");
        }
        _pollState = POLL_IDLE;
        return;
    }

    // No bytes yet — check for timeout
    if (now - _lastPollMs >= POLL_TIMEOUT_MS) {
        LOG("G5500", "POLL timeout - no response from G5500");
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
