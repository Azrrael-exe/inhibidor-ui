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

    // Send feedback poll (0xCC/0xC2), parse response.
    // Note: setTimeout(200) only takes effect if at least one byte has already arrived
    // in the RX buffer. If the G5500 has not started responding, available() returns
    // false immediately without waiting. 200ms is an upper bound, not a guarantee.
    // Returns false on timeout, parse failure, or missing keys.
    bool readStatus(RotorStatus& out);

private:
    HardwareSerial* _serial;
    DataPack        _txPack;  // member (182 bytes) — not stack-allocated
    DataPack        _rxPack;  // member (182 bytes) — not stack-allocated
};
