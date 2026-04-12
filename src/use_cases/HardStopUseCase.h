#pragma once
#include <Arduino.h>
#include "../services/RotorService.h"

class HardStopUseCase {
public:
    explicit HardStopUseCase(RotorService* service);
    void execute();

private:
    RotorService* _service;
};
