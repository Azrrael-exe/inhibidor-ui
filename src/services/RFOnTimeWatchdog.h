#pragma once
#include <Arduino.h>

class RFOnTimeWatchdog {
public:
    using Action = void (*)(void* ctx);

    RFOnTimeWatchdog(Action onExpire, void* context, unsigned long maxOnMs);

    // Notify per-band state after a digitalWrite on RF_BAND_x.
    // bandIdx must be in [0, NUM_BANDS).
    void notifyBand(uint8_t bandIdx, bool on);

    // Notify that all bands have been forced off (e.g. HardStop / Homming).
    void allOff();

    // Call once per loop().
    void update();

    bool isAnyOn() const { return _anyOn; }

    void          setMaxOnMs(unsigned long maxOnMs) { _maxOnMs = maxOnMs; }
    unsigned long getMaxOnMs() const                { return _maxOnMs; }

private:
    static constexpr uint8_t NUM_BANDS = 7;

    void recomputeAnyOn();

    bool          _bandOn[NUM_BANDS];
    bool          _anyOn;
    unsigned long _onSinceMs;
    unsigned long _maxOnMs;
    Action        _onExpire;
    void*         _context;
    bool          _tripped;
};
