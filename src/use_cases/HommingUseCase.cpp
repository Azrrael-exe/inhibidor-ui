#include "HommingUseCase.h"
#include "../pinout.h"

static const uint8_t BAND_PINS[7] = {
    RF_BAND_0, RF_BAND_1, RF_BAND_2, RF_BAND_3,
    RF_BAND_4, RF_BAND_5, RF_BAND_6
};

HommingUseCase::HommingUseCase(RotorService* service)
    : _service(service) {}

void HommingUseCase::execute() {
    // 1. Apagar todas las bandas de RF
    for (uint8_t i = 0; i < 7; i++) {
        digitalWrite(BAND_PINS[i], LOW);
    }
    if (_rfWatchdog) _rfWatchdog->allOff();

    // 2. Enviar comando de homing al rotor G5500 (key 0xFF, value 0x02)
    if (_service) {
        _service->home();
    }
}
