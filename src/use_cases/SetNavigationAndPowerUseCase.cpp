#include "SetNavigationAndPowerUseCase.h"
#include "../pinout.h"
#include <string.h>

static const uint8_t BAND_PINS[7] = {
    RF_BAND_0, RF_BAND_1, RF_BAND_2, RF_BAND_3,
    RF_BAND_4, RF_BAND_5, RF_BAND_6
};

SetNavigationAndPowerUseCase::SetNavigationAndPowerUseCase(RotorService* service)
    : _service(service) {}

bool SetNavigationAndPowerUseCase::execute(
    bool     hasAz,    float az,
    bool     hasEl,    float el,
    int8_t   bands[7],
    char*    errorMsg, uint8_t errorMsgLen
) {
    if (errorMsgLen == 0) return false;
    if (hasAz && (az < 0.0f || az > 360.0f)) {
        strncpy(errorMsg, "azimuth out of range [0,360]", errorMsgLen - 1);
        errorMsg[errorMsgLen - 1] = '\0';
        return false;
    }
    if (hasEl && (el < 0.0f || el > 90.0f)) {
        strncpy(errorMsg, "elevation out of range [0,90]", errorMsgLen - 1);
        errorMsg[errorMsgLen - 1] = '\0';
        return false;
    }

    _service->enqueuePosition(hasAz, az, hasEl, el);

    // bands[i] == -1 means the key was absent — leave that pin unchanged.
    for (uint8_t i = 0; i < 7; i++) {
        if (bands[i] >= 0) {
            digitalWrite(BAND_PINS[i], bands[i] ? HIGH : LOW);
        }
    }

    return true;
}
