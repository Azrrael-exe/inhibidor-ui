#include "RFOnTimeWatchdog.h"
#include "../logger.h"

RFOnTimeWatchdog::RFOnTimeWatchdog(Action onExpire, void* context, unsigned long maxOnMs)
    : _anyOn(false),
      _onSinceMs(0),
      _maxOnMs(maxOnMs),
      _onExpire(onExpire),
      _context(context),
      _tripped(false) {
    for (uint8_t i = 0; i < NUM_BANDS; i++) _bandOn[i] = false;
}

void RFOnTimeWatchdog::recomputeAnyOn() {
    bool any = false;
    for (uint8_t i = 0; i < NUM_BANDS; i++) {
        if (_bandOn[i]) { any = true; break; }
    }
    if (any && !_anyOn) {
        _onSinceMs = millis();
        _tripped   = false;
    } else if (!any && _anyOn) {
        _tripped = false;
    }
    _anyOn = any;
}

void RFOnTimeWatchdog::notifyBand(uint8_t bandIdx, bool on) {
    if (bandIdx >= NUM_BANDS) return;
    if (_bandOn[bandIdx] == on) return;
    _bandOn[bandIdx] = on;
    recomputeAnyOn();
}

void RFOnTimeWatchdog::allOff() {
    for (uint8_t i = 0; i < NUM_BANDS; i++) _bandOn[i] = false;
    if (_anyOn) {
        _anyOn   = false;
        _tripped = false;
    }
}

void RFOnTimeWatchdog::update() {
    if (!_anyOn || _tripped) return;
    if ((millis() - _onSinceMs) > _maxOnMs) {
        _tripped = true;
        LOG_F("RFOnTimeWatchdog", "Max RF on-time exceeded (ms): ", _maxOnMs);
        if (_onExpire) _onExpire(_context);
    }
}
