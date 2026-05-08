#pragma once
#include <Arduino.h>
#include "../services/RotorService.h"
#include "../services/RFOnTimeWatchdog.h"

class SetNavigationAndPowerUseCase {
public:
    explicit SetNavigationAndPowerUseCase(RotorService* service);

    void setRFOnTimeWatchdog(RFOnTimeWatchdog* wd) { _rfWatchdog = wd; }

    // bands[i]: -1 = absent (don't touch), 0 = LOW, 1 = HIGH.
    // errorMsg: caller-owned stack buffer written on validation failure. errorMsgLen must be > 0.
    // Precondition: all RF_BAND_x pins must be configured as OUTPUT before first call.
    // Returns true on success, false if validation fails.
    bool execute(
        bool     hasAz,    float az,
        bool     hasEl,    float el,
        int8_t   bands[7],
        char*    errorMsg, uint8_t errorMsgLen
    );

private:
    RotorService*     _service;
    RFOnTimeWatchdog* _rfWatchdog = nullptr;
};
