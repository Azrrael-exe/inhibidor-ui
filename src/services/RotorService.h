#pragma once
#include <Arduino.h>
#include <llp.h>

struct RotorStatus {
    float azimuthAngle;    // degrees (G5500 key 0xAB, raw uint16_t / 10.0)
    float elevationAngle;  // degrees (G5500 key 0xBC, raw uint16_t / 10.0)
};

struct RotorCommand {
    bool  hasAz;
    float az;
    bool  hasEl;
    float el;
    bool  pending;
};

class RotorService {
public:
    // pollIntervalMs: how often to request a status update from the G5500.
    explicit RotorService(HardwareSerial* serial, unsigned long pollIntervalMs = 2000UL);

    // Enqueue a goto command (az and/or el). Overwrites any previously pending
    // command — only the last call before the next update() is sent.
    // Encoded as int16_t * 10 in a single LLP frame (keys 0xDA and/or 0xDB).
    void enqueuePosition(bool hasAz, float az, bool hasEl, float el);

    // key 0xFF, value 0x01 — Highest priority stop sequence
    void emergencyKill(); 
    
    void stopAzimuth();   // key 0xAA, value 0xA0 — fire-and-forget, sent immediately
    void stopElevation(); // key 0xBB, value 0xB0 — fire-and-forget, sent immediately

    // Async status polling — call every loop().
    // Drains any pending command first, then sends a status poll every pollIntervalMs.
    // Non-blocking: never waits for Serial bytes.
    void update();

    // Returns true if at least one valid status has been received.
    bool hasStatus() const;

    // Returns the last cached rotor status. Check hasStatus() first.
    const RotorStatus& getStatus() const;

    // Returns true when Serial is not actively used by the rotor (POLL_IDLE).
    // Use this to gate ASCII logs — printing during POLL_SENT sends bytes
    // on the G5500 TX line that corrupt LLP framing.
    bool isSerialFree() const;

private:
    HardwareSerial* _serial;
    DataPack        _txPack;  // member (182 bytes) — not stack-allocated
    DataPack        _rxPack;  // member (182 bytes) — not stack-allocated

    enum PollState : uint8_t { POLL_IDLE, POLL_SENT };
    PollState     _pollState;
    unsigned long _lastPollMs;
    unsigned long _pollIntervalMs;
    RotorStatus   _cachedStatus;
    bool          _hasStatus;
    RotorCommand  _pendingCmd;

    static const unsigned long POLL_TIMEOUT_MS = 500UL;
};
