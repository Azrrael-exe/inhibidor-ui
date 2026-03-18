#include "RotorService.h"

// Set to 1 to enable G5500 debug logs on Serial.
// Keep at 0 in production — logs share the Serial port with G5500 binary frames.
#define ROTOR_DEBUG 1

#if ROTOR_DEBUG
#  define ROTOR_LOG(msg)       Serial.print(F("[G5500] ")); Serial.println(F(msg))
#  define ROTOR_LOG_F(msg, v)  Serial.print(F("[G5500] ")); Serial.print(F(msg)); Serial.println(v)
#else
#  define ROTOR_LOG(msg)
#  define ROTOR_LOG_F(msg, v)
#endif

RotorService::RotorService(HardwareSerial* serial)
    : _serial(serial)
    , _pollState(POLL_IDLE)
    , _lastPollMs(0)
    , _cachedStatus({0.0f, 0.0f})
    , _hasStatus(false) {}

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

void RotorService::update() {
    unsigned long now = millis();

    if (_pollState == POLL_IDLE) {
        if (now - _lastPollMs >= POLL_INTERVAL_MS) {
            while (_serial->available()) _serial->read();  // flush stale bytes
            ROTOR_LOG("CMD poll_status");
            _txPack.clear();
            _txPack.addData(0xCC, (int16_t)0xC2);
            _txPack.write(*_serial);
            _lastPollMs = now;
            _pollState  = POLL_SENT;
        }
        return;
    }

    // POLL_SENT: check if response has arrived (non-blocking)
    if (_serial->available()) {
        if (_rxPack.available(*_serial)) {
            if (_rxPack.hasKey(0xAB) && _rxPack.hasKey(0xBC)) {
                // getData returns uint16_t; G5500 angles are always non-negative so this is safe.
                _cachedStatus.azimuthAngle   = _rxPack.getData(0xAB) / 10.0f;
                _cachedStatus.elevationAngle = _rxPack.getData(0xBC) / 10.0f;
                _hasStatus = true;
#if ROTOR_DEBUG
                Serial.print(F("[G5500] STATUS az="));
                Serial.print(_cachedStatus.azimuthAngle);
                Serial.print(F(" el="));
                Serial.println(_cachedStatus.elevationAngle);
#endif
            } else {
                ROTOR_LOG("STATUS missing keys");
            }
        } else {
            ROTOR_LOG("STATUS parse error");
        }
        _pollState = POLL_IDLE;
        return;
    }

    // No bytes yet — check for timeout
    if (now - _lastPollMs > POLL_TIMEOUT_MS) {
        ROTOR_LOG("STATUS timeout (no response)");
        _pollState = POLL_IDLE;
    }
}

bool RotorService::hasStatus() const {
    return _hasStatus;
}

const RotorStatus& RotorService::getStatus() const {
    return _cachedStatus;
}
