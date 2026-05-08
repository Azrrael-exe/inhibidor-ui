#pragma once
#include <Arduino.h>
#include "../services/RotorService.h"
#include "../services/RFOnTimeWatchdog.h"

class HardStopUseCase {
public:
    explicit HardStopUseCase(RotorService* service);
    void execute();

    void setRFOnTimeWatchdog(RFOnTimeWatchdog* wd) { _rfWatchdog = wd; }

private:
    RotorService*     _service;
    RFOnTimeWatchdog* _rfWatchdog = nullptr;
};
