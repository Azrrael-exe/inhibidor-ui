#pragma once
#include <Arduino.h>
#include "../services/RotorService.h"
#include "../services/RFOnTimeWatchdog.h"

class HommingUseCase {
public:
    explicit HommingUseCase(RotorService* service);
    void execute();

    void setRFOnTimeWatchdog(RFOnTimeWatchdog* wd) { _rfWatchdog = wd; }

private:
    RotorService*     _service;
    RFOnTimeWatchdog* _rfWatchdog = nullptr;
};
