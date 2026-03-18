#pragma once
#include <Arduino.h>
#include <llp.h>

struct RotorStatus {
    float azimuthAngle;    // degrees (G5500 key 0xAB, raw uint16_t / 10.0)
    float elevationAngle;  // degrees (G5500 key 0xBC, raw uint16_t / 10.0)
};

class RotorService {
public:
    explicit RotorService(HardwareSerial* serial);

    // Send goto commands (angle encoded as int16_t * 10)
    void gotoAzimuth(float degrees);    // key 0xDA
    void gotoElevation(float degrees);  // key 0xDB
    void stopAzimuth();                 // key 0xAA, value 0xA0
    void stopElevation();               // key 0xBB, value 0xB0

    // Async status polling — call every loop().
    // Sends a feedback poll every POLL_INTERVAL_MS and caches the response.
    // Non-blocking: never waits for Serial bytes.
    void update();

    // Returns true if at least one valid status has been received.
    bool hasStatus() const;

    // Returns the last cached rotor status. Check hasStatus() first.
    const RotorStatus& getStatus() const;

private:
    HardwareSerial* _serial;
    DataPack        _txPack;  // member (182 bytes) — not stack-allocated
    DataPack        _rxPack;  // member (182 bytes) — not stack-allocated

    enum PollState : uint8_t { POLL_IDLE, POLL_SENT };
    PollState     _pollState;
    unsigned long _lastPollMs;
    RotorStatus   _cachedStatus;
    bool          _hasStatus;

    static const unsigned long POLL_INTERVAL_MS = 2000UL;
    static const unsigned long POLL_TIMEOUT_MS  =  500UL;
};
