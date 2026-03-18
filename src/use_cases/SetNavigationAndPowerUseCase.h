#pragma once
#include <Arduino.h>
#include "../services/RotorService.h"

class SetNavigationAndPowerUseCase {
public:
    explicit SetNavigationAndPowerUseCase(RotorService* service);

    // bands[i]: -1 = absent (don't touch), 0 = LOW, 1 = HIGH.
    // errorMsg: caller-owned stack buffer written on validation failure.
    // Returns true on success, false if validation fails.
    bool execute(
        bool     hasAz,    float az,
        bool     hasEl,    float el,
        int8_t   bands[7],
        char*    errorMsg, uint8_t errorMsgLen
    );

private:
    RotorService* _service;
};
